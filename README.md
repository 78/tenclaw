# TenBox

A Windows-native Virtual Machine Monitor (VMM) built on the Windows Hypervisor Platform (WHVP). TenBox runs full Linux desktop environments with hardware-accelerated virtualization, GPU display output, audio, shared folders, and clipboard integration — all managed through a native Win32 GUI.

## Features

- **WHVP hypervisor backend** — native hardware-accelerated virtualization on Windows 10/11
- **GUI manager** — Win32 application with system tray, multi-VM management, and display window
- **Linux boot protocol** — boots standard `vmlinuz` + `initramfs`
- **VirtIO MMIO devices** — block, network, GPU, input, serial, sound, and filesystem
- **qcow2 & raw disk images** — zlib and zstd compressed cluster support, copy-on-write
- **GPU display** — virtio-gpu with SPICE protocol, resizable display window
- **Audio output** — virtio-snd streamed to host via WASAPI
- **Shared folders** — virtiofs (virtio-fs), configurable per VM with optional read-only mode
- **Clipboard sharing** — bidirectional host ↔ guest clipboard via SPICE vdagent protocol
- **Guest agent** — qemu-guest-agent integration for VM lifecycle management
- **NAT networking** — built-in DHCP server, TCP/UDP NAT proxy, ICMP relay via lwIP
- **Port forwarding** — expose guest TCP services on host ports
- **Multi-VM management** — create, edit, start, stop, reboot, and delete VMs; config persisted as `vm.json`
- **Minimal device emulation** — UART 16550, i8254 PIT, CMOS RTC, I/O APIC, i8259 PIC, ACPI PM

## Quick Start

### Prerequisites

- Windows 10/11 with **Hyper-V** enabled (for WHVP)
- Visual Studio 2022+ with C++20 support
- CMake 3.21+
- WSL2 or a Linux environment (for building disk images)

### Build

```bash
cmake -B out/build -G Ninja
cmake --build out/build
```

This produces two executables in the build output directory:

| Executable | Description |
|---|---|
| `tenbox-manager.exe` | GUI manager — the main entry point |
| `tenbox-vm-runtime.exe` | VM runtime process — launched by the manager |

### Prepare VM Images

Run these scripts in WSL2 or Linux:

```bash
# 1. Get a Debian kernel
./scripts/get-kernel.sh

# 2. Build initramfs (includes virtio + ext4 modules)
./scripts/make-initramfs.sh

# 3. Build a Debian rootfs (Debian Bookworm, XFCE desktop, ~20 GB qcow2)
sudo ./scripts/make-rootfs.sh
```

The rootfs script supports incremental builds with a checkpoint system. If interrupted, re-run the same command to resume:

```bash
./scripts/make-rootfs.sh --status       # Show build progress
./scripts/make-rootfs.sh --list-steps   # List all build steps
./scripts/make-rootfs.sh --force        # Force full rebuild
./scripts/make-rootfs.sh --edit         # Modify an existing rootfs
```

### Run

Launch the manager, then use the GUI to create and start VMs:

```bash
tenbox-manager.exe
```

To create a VM through the GUI, click **New VM** and point to the kernel, initramfs, and disk image files built above.

## Architecture

TenBox uses a two-process design. The manager process owns the UI and spawns a separate runtime process for each VM. They communicate over a Windows Named Pipe.

```
┌──────────────────────────────────────────────────────────────────┐
│  tenbox-manager.exe                                              │
│                                                                  │
│  Win32 GUI                                                       │
│  ├─ VM list, toolbar, system tray                                │
│  ├─ Display window (virtio-gpu frames via IPC)                   │
│  ├─ Console tab (serial I/O via IPC)                             │
│  ├─ Clipboard bridge (host ↔ vdagent)                           │
│  └─ WASAPI audio player                                          │
│                  │ Named Pipe (IPC protocol v1)                  │
│                  ▼                                               │
│  tenbox-vm-runtime.exe  [one per running VM]                     │
│  ├─ WHVP VM + vCPUs                                              │
│  ├─ Address space (PIO / MMIO)                                   │
│  ├─ Devices: UART · PIT · RTC · IOAPIC · PIC · ACPI · PCI       │
│  ├─ VirtIO MMIO: blk · net · gpu · input · serial · snd · fs    │
│  ├─ vdagent handler (clipboard)                                  │
│  ├─ guest_agent handler                                          │
│  └─ Net backend: lwIP · DHCP · NAT · port forward               │
└──────────────────────────────────────────────────────────────────┘
```

### Source Layout

```
src/
├── common/              # Shared types: VmSpec, PortForward, SharedFolder
├── core/                # VM engine (hypervisor-agnostic)
│   ├── arch/x86_64/     # Linux boot protocol, ACPI tables
│   ├── device/
│   │   ├── serial/      # UART 16550
│   │   ├── timer/       # i8254 PIT
│   │   ├── rtc/         # CMOS RTC
│   │   ├── irq/         # I/O APIC & i8259 PIC
│   │   ├── acpi/        # ACPI PM registers
│   │   ├── pci/         # PCI host bridge
│   │   └── virtio/      # VirtIO MMIO + blk/net/gpu/input/serial/snd/fs
│   ├── guest_agent/     # qemu-guest-agent protocol handler
│   ├── net/             # lwIP NAT backend
│   ├── vdagent/         # SPICE vdagent (clipboard protocol)
│   └── vmm/             # VM orchestration & address space
├── hypervisor/          # WHVP platform interface
├── ipc/                 # Named pipe protocol (manager ↔ runtime)
├── manager/             # GUI manager application
├── platform/            # Windows platform backends
│   ├── tray/            # System tray integration
│   └── windows/
│       ├── audio/       # WASAPI audio output
│       └── console/     # Console I/O
├── runtime/             # VM runtime process entry point & CLI
└── ui/
    ├── common/          # i18n helpers
    └── win32/           # Win32 display window, dialogs, components
        └── components/  # VM list, console tab, info tab
scripts/
├── get-kernel.sh        # Extract vmlinuz from a Debian package
├── make-initramfs.sh    # Build BusyBox initramfs with kernel modules
├── make-rootfs.sh       # Build Debian Bookworm rootfs as qcow2
├── extra-modules/       # Additional kernel modules to bundle
├── rootfs-scripts/      # In-chroot setup scripts
└── rootfs-services/     # systemd service units installed into the rootfs
```

### Networking

When NAT is enabled, TenBox provides a user-mode network:

| Address | Role |
|---|---|
| `10.0.2.2` | Gateway (host) |
| `10.0.2.15` | Guest IP (via DHCP) |
| `8.8.8.8` | DNS server |

- **Outbound TCP** — proxied through the lwIP TCP stack to host Winsock sockets
- **Outbound UDP** — directly relayed via Winsock (DNS, NTP, etc.)
- **ICMP** — relayed via raw socket (requires admin for ping)
- **Port forwarding** — configurable per VM; e.g., host port 2222 → guest port 22

### Guest Defaults (built by `make-rootfs.sh`)

| Setting | Default | Override |
|---|---|---|
| Root password | `tenbox` | `ROOT_PASSWORD` env var |
| User account | `openclaw` / `openclaw` | `USER_NAME` / `USER_PASSWORD` env var |
| Hostname | `tenbox-vm` | — |
| Desktop | XFCE 4 (LightDM) | — |
| Disk size | 20 GB qcow2 | `ROOTFS_SIZE` variable |
| Distro | Debian Bookworm | — |
| Pre-installed | Chrome, Node.js 22, SPICE vdagent, qemu-guest-agent | — |

## VM Runtime CLI

The runtime is normally launched by the manager, but can also be invoked directly:

```bash
tenbox-vm-runtime.exe --kernel build/vmlinuz --initrd build/initramfs.cpio.gz \
    --disk build/rootfs.qcow2 --net
```

| Option | Description |
|---|---|
| `--kernel <path>` | Path to vmlinuz **(required)** |
| `--initrd <path>` | Path to initramfs |
| `--disk <path>` | Path to raw or qcow2 disk image |
| `--cmdline <str>` | Kernel command line |
| `--memory <MB>` | Guest RAM in MB (default: 256) |
| `--cpus <N>` | Number of vCPUs (default: 1, max: 128) |
| `--net` | Enable virtio-net with NAT |
| `--forward H:G` | Forward host port H to guest port G (repeatable) |
| `--share TAG:PATH[:ro]` | Share a host directory via virtiofs (repeatable) |
| `--interactive on\|off` | Enable serial console I/O (default: on) |
| `--vm-id <id>` | VM instance identifier (default: `default`) |
| `--control-endpoint <name>` | Named pipe endpoint for manager IPC |

## Dependencies

Fetched automatically by CMake:

| Library | Use |
|---|---|
| [zlib](https://github.com/madler/zlib) | qcow2 zlib compressed cluster decompression |
| [zstd](https://github.com/facebook/zstd) | qcow2 zstd compressed cluster decompression |
| [lwIP](https://github.com/lwip-tcpip/lwip) | Lightweight TCP/IP stack for NAT networking |
| [nlohmann/json](https://github.com/nlohmann/json) | VM manifest (`vm.json`) serialization |

## License

GPL v3 — see [LICENSE](LICENSE) for details.
