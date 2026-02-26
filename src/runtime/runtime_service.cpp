#include "runtime/runtime_service.h"

#include "core/vmm/types.h"
#include "core/vmm/vm.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

HANDLE AsHandle(void* handle) {
    return reinterpret_cast<HANDLE>(handle);
}

}  // namespace

void ManagedConsolePort::Write(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    std::function<void(const uint8_t*, size_t)> handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler = write_handler_;
    }
    if (handler) {
        handler(data, size);
    }
}

size_t ManagedConsolePort::Read(uint8_t* out, size_t size) {
    if (!out || size == 0) return 0;
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        cv_.wait_for(lock, std::chrono::milliseconds(16));
    }
    size_t copied = 0;
    while (!queue_.empty() && copied < size) {
        out[copied++] = queue_.front();
        queue_.pop_front();
    }
    return copied;
}

void ManagedConsolePort::PushInput(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < size; ++i) {
            queue_.push_back(data[i]);
        }
    }
    cv_.notify_all();
}

void ManagedConsolePort::SetWriteHandler(
    std::function<void(const uint8_t*, size_t)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_handler_ = std::move(handler);
}

std::string EncodeHex(const uint8_t* data, size_t size) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.resize(size * 2);
    for (size_t i = 0; i < size; ++i) {
        out[2 * i] = kDigits[(data[i] >> 4) & 0x0F];
        out[2 * i + 1] = kDigits[data[i] & 0x0F];
    }
    return out;
}

std::vector<uint8_t> DecodeHex(const std::string& value) {
    auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    if ((value.size() % 2) != 0) return {};
    std::vector<uint8_t> out(value.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        int hi = hex(value[2 * i]);
        int lo = hex(value[2 * i + 1]);
        if (hi < 0 || lo < 0) return {};
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return out;
}

RuntimeControlService::RuntimeControlService(std::string vm_id, std::string pipe_name)
    : vm_id_(std::move(vm_id)), pipe_name_(std::move(pipe_name)) {
    console_port_->SetWriteHandler([this](const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(console_buf_mutex_);
        console_buf_.append(reinterpret_cast<const char*>(data), size);
    });
}

RuntimeControlService::~RuntimeControlService() {
    Stop();
}

bool RuntimeControlService::Start() {
    if (running_) return true;
    running_ = true;
    thread_ = std::thread(&RuntimeControlService::RunLoop, this);
    return true;
}

void RuntimeControlService::Stop() {
    running_ = false;
    HANDLE h = AsHandle(pipe_handle_);
    if (h && h != INVALID_HANDLE_VALUE) {
        CancelIoEx(h, nullptr);
        DisconnectNamedPipe(h);
        CloseHandle(h);
        pipe_handle_ = nullptr;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RuntimeControlService::AttachVm(Vm* vm) {
    vm_ = vm;
}

void RuntimeControlService::PublishState(const std::string& state, int exit_code) {
    ipc::Message event;
    event.kind = ipc::Kind::kEvent;
    event.channel = ipc::Channel::kControl;
    event.type = "runtime.state";
    event.vm_id = vm_id_;
    event.request_id = next_event_id_++;
    event.fields["state"] = state;
    event.fields["exit_code"] = std::to_string(exit_code);
    Send(event);
}

bool RuntimeControlService::EnsureClientConnected() {
    if (pipe_name_.empty()) return false;

    HANDLE h = AsHandle(pipe_handle_);
    if (h && h != INVALID_HANDLE_VALUE) return true;

    std::string full_name = R"(\\.\pipe\)" + pipe_name_;
    h = CreateNamedPipeA(
        full_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        64 * 1024,
        64 * 1024,
        0,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        LOG_ERROR("CreateNamedPipe failed for %s", full_name.c_str());
        return false;
    }

    BOOL connected = ConnectNamedPipe(h, nullptr);
    if (!connected) {
        DWORD err = GetLastError();
        if (err != ERROR_PIPE_CONNECTED) {
            CloseHandle(h);
            LOG_ERROR("ConnectNamedPipe failed: %lu", err);
            return false;
        }
    }

    pipe_handle_ = h;
    return true;
}

bool RuntimeControlService::Send(const ipc::Message& message) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    HANDLE h = AsHandle(pipe_handle_);
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    std::string encoded = ipc::Encode(message);
    DWORD written = 0;
    if (!WriteFile(h, encoded.data(), static_cast<DWORD>(encoded.size()), &written, nullptr)) {
        return false;
    }
    return written == encoded.size();
}

void RuntimeControlService::HandleMessage(const ipc::Message& message) {
    if (message.channel == ipc::Channel::kControl &&
        message.kind == ipc::Kind::kRequest &&
        message.type == "runtime.command") {
        ipc::Message resp;
        resp.kind = ipc::Kind::kResponse;
        resp.channel = ipc::Channel::kControl;
        resp.type = "runtime.command.result";
        resp.vm_id = vm_id_;
        resp.request_id = message.request_id;
        resp.fields["ok"] = "true";

        auto it = message.fields.find("command");
        if (it == message.fields.end()) {
            resp.fields["ok"] = "false";
            resp.fields["error"] = "missing command";
            Send(resp);
            return;
        }
        const std::string& cmd = it->second;
        if (cmd == "stop") {
            if (vm_) vm_->RequestStop();
        } else if (cmd == "shutdown") {
            if (vm_) {
                vm_->TriggerPowerButton();
                static const char kPoweroff[] = "\npoweroff\n";
                vm_->InjectConsoleBytes(
                    reinterpret_cast<const uint8_t*>(kPoweroff),
                    sizeof(kPoweroff) - 1);
            }
        } else if (cmd == "reboot") {
            if (vm_) vm_->RequestStop();
            resp.fields["note"] = "reboot not implemented, performed stop";
        } else if (cmd == "start") {
            resp.fields["note"] = "runtime already started by process launch";
        } else {
            resp.fields["ok"] = "false";
            resp.fields["error"] = "unknown command";
        }
        Send(resp);
        return;
    }

    if (message.channel == ipc::Channel::kControl &&
        message.kind == ipc::Kind::kRequest &&
        message.type == "runtime.update_network") {
        ipc::Message resp;
        resp.kind = ipc::Kind::kResponse;
        resp.channel = ipc::Channel::kControl;
        resp.type = "runtime.update_network.result";
        resp.vm_id = vm_id_;
        resp.request_id = message.request_id;

        if (!vm_) {
            resp.fields["ok"] = "false";
            resp.fields["error"] = "vm not attached";
            Send(resp);
            return;
        }

        auto it_link = message.fields.find("link_up");
        if (it_link != message.fields.end()) {
            vm_->SetNetLinkUp(it_link->second == "true");
        }

        auto it_count = message.fields.find("forward_count");
        if (it_count != message.fields.end()) {
            int count = 0;
            auto [p, ec] = std::from_chars(
                it_count->second.data(),
                it_count->second.data() + it_count->second.size(), count);
            if (ec == std::errc{} && count >= 0) {
                std::vector<PortForward> forwards;
                for (int i = 0; i < count; ++i) {
                    auto it_f = message.fields.find("forward_" + std::to_string(i));
                    if (it_f == message.fields.end()) continue;
                    unsigned hp = 0, gp = 0;
                    if (std::sscanf(it_f->second.c_str(), "%u:%u", &hp, &gp) == 2
                        && hp && gp) {
                        forwards.push_back({static_cast<uint16_t>(hp),
                                            static_cast<uint16_t>(gp)});
                    }
                }
                vm_->UpdatePortForwards(forwards);
            }
        }

        resp.fields["ok"] = "true";
        Send(resp);
        return;
    }

    if (message.channel == ipc::Channel::kConsole &&
        message.kind == ipc::Kind::kRequest &&
        message.type == "console.input") {
        auto it = message.fields.find("data_hex");
        if (it != message.fields.end()) {
            auto bytes = DecodeHex(it->second);
            console_port_->PushInput(bytes.data(), bytes.size());
        }
        return;
    }

    if (message.channel == ipc::Channel::kControl &&
        message.kind == ipc::Kind::kRequest &&
        message.type == "runtime.ping") {
        ipc::Message resp;
        resp.kind = ipc::Kind::kResponse;
        resp.channel = ipc::Channel::kControl;
        resp.type = "runtime.pong";
        resp.vm_id = vm_id_;
        resp.request_id = message.request_id;
        Send(resp);
    }
}

void RuntimeControlService::FlushConsoleBuf() {
    std::string data;
    {
        std::lock_guard<std::mutex> lock(console_buf_mutex_);
        if (console_buf_.empty()) return;
        data.swap(console_buf_);
    }

    ipc::Message event;
    event.kind = ipc::Kind::kEvent;
    event.channel = ipc::Channel::kConsole;
    event.type = "console.data";
    event.vm_id = vm_id_;
    event.request_id = next_event_id_++;
    event.fields["data_hex"] = EncodeHex(
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
    Send(event);
}

void RuntimeControlService::RunLoop() {
    if (!EnsureClientConnected()) {
        return;
    }

    HANDLE h = AsHandle(pipe_handle_);
    std::array<char, 4096> buf{};
    std::string pending;
    last_console_flush_ = std::chrono::steady_clock::now();

    while (running_) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_console_flush_ >= std::chrono::milliseconds(16)) {
            FlushConsoleBuf();
            last_console_flush_ = now;
        }

        DWORD available = 0;
        if (!PeekNamedPipe(h, nullptr, 0, nullptr, &available, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                break;
            }
            Sleep(1);
            continue;
        }
        if (available == 0) {
            Sleep(1);
            continue;
        }

        DWORD to_read = (std::min)(available, static_cast<DWORD>(buf.size()));
        DWORD read = 0;
        if (!ReadFile(h, buf.data(), to_read, &read, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_OPERATION_ABORTED) {
                break;
            }
            continue;
        }
        if (read == 0) {
            continue;
        }

        pending.append(buf.data(), read);
        size_t pos = 0;
        while (true) {
            size_t nl = pending.find('\n', pos);
            if (nl == std::string::npos) {
                pending.erase(0, pos);
                break;
            }
            std::string line = pending.substr(pos, nl - pos + 1);
            pos = nl + 1;
            auto decoded = ipc::Decode(line);
            if (decoded) {
                HandleMessage(*decoded);
            }
        }
    }

    FlushConsoleBuf();
}
