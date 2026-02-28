#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class VirtioSerialDevice;

class GuestAgentHandler {
public:
    using ConnectedCallback = std::function<void(bool connected)>;

    GuestAgentHandler();
    ~GuestAgentHandler();

    void SetSerialDevice(VirtioSerialDevice* device, uint32_t port_id);
    void SetConnectedCallback(ConnectedCallback cb);

    // Called when the guest opens/closes the port
    void OnPortOpen(bool opened);

    // Called when raw data arrives from the guest agent
    void OnDataReceived(const uint8_t* data, size_t len);

    bool IsConnected() const { return connected_.load(); }

    // Send a guest-shutdown command (mode: "halt", "powerdown", or "reboot")
    void Shutdown(const std::string& mode = "powerdown");

    // Send a guest-ping command; returns true if the request was sent
    void Ping();

private:
    void SendCommand(const std::string& command);
    void SendCommand(const std::string& command,
                     const std::string& arguments_json);
    void SendRaw(const std::string& json_line);
    void ProcessLine(const std::string& line);
    void StartSyncHandshake();

    VirtioSerialDevice* serial_device_ = nullptr;
    uint32_t port_id_ = 0;

    std::mutex mutex_;
    std::string recv_buffer_;
    std::atomic<bool> connected_{false};
    bool sync_pending_ = false;
    int64_t sync_id_ = 0;
    uint64_t next_id_ = 1;
    ConnectedCallback connected_callback_;
};
