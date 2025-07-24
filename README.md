# HVD - Hypervisor Management Daemon for FreeBSD BHyve

HVD is a comprehensive hypervisor management system for FreeBSD BHyve that provides VM lifecycle management, network configuration, and integration with the netd network management daemon.

## Features

- **VM Lifecycle Management**: Create, start, stop, and destroy BHyve virtual machines
- **ZFS Integration**: Automatic ZFS dataset and zvol management for VM storage
- **Network Management**: Bridge network creation and tap interface management
- **netd Integration**: Seamless integration with the netd network management daemon
- **XML Configuration**: YANG-based configuration management using BSDXML
- **FIB Support**: Full support for FreeBSD's FIB (routing table) management
- **Interactive CLI**: Rich command-line interface with tab completion
- **One-shot Commands**: Support for scripting and automation

## Architecture

The system consists of two main components:

- **hvd**: The server daemon that manages VMs and networks
- **hv**: The CLI client for interacting with the daemon

### File System Structure

```
/hv/
├── vm/
│   ├── <vm_name>/
│   │   ├── config.xml          # VM configuration
│   │   ├── disks/              # ZFS subvolume for VM disks
│   │   │   ├── boot.zvol       # Boot disk (ZFS zvol)
│   │   │   └── data.zvol       # Data disk (ZFS zvol)
│   │   └── state/              # VM runtime state
│   └── networks/
│       ├── <network_name>/
│       │   ├── config.xml      # Network configuration
│       │   └── bridge/         # Bridge interface config
└── config/
    └── global.xml              # Global hypervisor configuration
```

## Installation

### Prerequisites

- FreeBSD 13.0 or later
- ZFS filesystem
- BHyve hypervisor support
- netd (optional, for network integration)

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd hvd

# Build the project
make

# Install (requires root privileges)
sudo make install
```

### Manual Installation

```bash
# Copy binaries
sudo cp hvd /usr/local/sbin/
sudo cp hv /usr/local/bin/

# Copy YANG models
sudo mkdir -p /usr/local/share/yang
sudo cp yang/hvd-vm.yang /usr/local/share/yang/
sudo cp yang/hvd-network.yang /usr/local/share/yang/
```

## Usage

### Starting the Daemon

```bash
# Start the HVD daemon
sudo hvd
```

The daemon will:
1. Initialize the ZFS structure under `/hv`
2. Check for netd integration availability
3. Start listening on `/var/run/hvd.sock`

### Interactive Mode

```bash
# Start interactive CLI
hv

# Example session
hv> create vm test 2 1024
hv> create network lan 0 em0
hv> set vm test cpu 4
hv> start test
hv> list vm
hv> show vm test
```

### One-shot Commands

```bash
# Create a VM
hv create vm test 2 1024

# Create a network
hv create network lan 0 em0

# Set VM properties
hv set vm test cpu 4
hv set vm test memory 2048

# Start/stop VMs
hv start test
hv stop test

# List resources
hv list vm
hv list network

# Show details
hv show vm test
hv show network lan
```

## Commands Reference

### VM Management

| Command | Description |
|---------|-------------|
| `create vm <name> <cpu> <memory>` | Create a new VM |
| `destroy vm <name>` | Destroy a VM |
| `start <vm_name>` | Start a VM |
| `stop <vm_name>` | Stop a VM |
| `set vm <name> <prop> <value>` | Set VM property |
| `show vm <name>` | Show VM details |
| `list vm` | List all VMs |

### Network Management

| Command | Description |
|---------|-------------|
| `create network <name> <fib> [if]` | Create a new network |
| `destroy network <name>` | Destroy a network |
| `set network <name> <prop> <value>` | Set network property |
| `show network <name>` | Show network details |
| `list network` | List all networks |

### VM Properties

| Property | Type | Range | Description |
|----------|------|-------|-------------|
| `cpu` | integer | 1-32 | Number of CPU cores |
| `memory` | integer | 64-1048576 | Memory in MB |
| `boot-device` | string | - | Boot device name |

### Network Properties

| Property | Type | Range | Description |
|----------|------|-------|-------------|
| `fib` | integer | 0-255 | FIB (routing table) ID |
| `physical-interface` | string | - | Physical interface name |

## Configuration

### YANG Models

The system uses YANG models for configuration validation and schema definition:

- `hvd-vm.yang`: VM configuration schema
- `hvd-network.yang`: Network configuration schema

### XML Configuration Files

VM and network configurations are stored as XML files:

- VM config: `/hv/vm/<vm_name>/config.xml`
- Network config: `/hv/networks/<network_name>/config.xml`

## Integration with netd

HVD integrates with the netd network management daemon to:

- Create and configure bridge interfaces
- Manage tap interfaces for VM networking
- Set FIB assignments for network isolation
- Configure physical interface attachments

The integration is optional - HVD will continue to function without netd, but network management features will be limited.

## ZFS Integration

HVD automatically manages ZFS datasets for:

- VM storage directories
- Network configuration directories
- ZFS zvols for VM disks
- Configuration metadata via ZFS properties

## Development

### Building for Development

```bash
# Build with debug symbols
make CFLAGS="-Wall -Wextra -std=c99 -D_BSD_SOURCE -g -O0"

# Run with debugging
sudo gdb --args hvd
```

### Project Structure

```
src/
├── common.h              # Shared definitions and structures
├── hvd.c                 # Main server daemon
├── hv_client.c          # CLI client
├── vm_manager.c         # VM lifecycle operations
├── network_manager.c    # Network management
├── zfs_manager.c        # ZFS operations
├── xml_config.c         # XML configuration management
├── netd_integration.c   # netd integration
└── commands.c           # Command parsing and routing

yang/
├── hvd-vm.yang         # VM configuration schema
└── hvd-network.yang    # Network configuration schema
```

## Troubleshooting

### Common Issues

1. **Permission Denied**: Ensure the daemon is running with appropriate privileges
2. **ZFS Dataset Creation Fails**: Check ZFS pool availability and permissions
3. **netd Integration Fails**: Verify netd is running and accessible
4. **VM Start Fails**: Check BHyve support and kernel modules

### Debugging

```bash
# Check daemon status
ps aux | grep hvd

# Check socket
ls -la /var/run/hvd.sock

# Check ZFS datasets
zfs list -r /hv

# Check netd integration
net show version
```

## License

This project is licensed under the BSD 3-Clause License. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Support

For support and questions:

- Open an issue on GitHub
- Check the documentation
- Review the troubleshooting section

## Roadmap

- [ ] iSCSI disk support
- [ ] VM snapshots and backups
- [ ] Advanced networking features (VLAN, etc.)
- [ ] Web-based management interface
- [ ] Monitoring and statistics
- [ ] Resource quotas and limits 