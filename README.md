# LiteRun

[![GitHub release](https://img.shields.io/github/v/release/svtica/LiteRun)](https://github.com/svtica/LiteRun/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/svtica/LiteRun/build-and-release.yml)](https://github.com/svtica/LiteRun/actions)
[![Part of LiteSuite](https://img.shields.io/badge/part%20of-LiteSuite-blue)](https://github.com/svtica/LiteSuite)
[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-green.svg)](LICENSE)

**Lightweight remote execution utility for running commands and applications on remote Windows systems.**

LiteRun is a command-line utility and library designed for executing programs and commands on remote Windows systems with minimal overhead and maximum flexibility.

## Features

### Remote Execution
- **Command Execution**: Run any command or executable on remote Windows systems
- **Interactive Sessions**: Support for interactive command sessions
- **Output Capture**: Real-time capture of stdout, stderr, and exit codes
- **Process Control**: Start, monitor, and terminate remote processes

### Authentication & Security
- **Multiple Auth Methods**: Support for various Windows authentication mechanisms
- **Credential Management**: Secure credential handling and storage
- **Context Switching**: Execute commands under different user contexts
- **Encrypted Communication**: Secure data transmission over network

### Performance & Reliability
- **Lightweight Design**: Minimal resource footprint on both local and remote systems
- **Connection Pooling**: Efficient connection management for multiple operations
- **Error Handling**: Robust error detection and recovery mechanisms
- **Timeout Management**: Configurable timeouts for operations

### Integration
- **Command Line Interface**: Easy integration with scripts and automation tools
- **Library Mode**: Can be used as a component in other applications
- **Batch Operations**: Execute commands across multiple remote systems
- **Logging**: Comprehensive logging for audit and troubleshooting

## Installation

1. Download the latest release
2. Extract files to desired location
3. Add to PATH environment variable (optional)
4. Run `LiteRun.exe` from command line

## Usage

### Basic Command Execution
```cmd
LiteRun.exe -server COMPUTER -command "dir C:\"
```

### With Credentials
```cmd
LiteRun.exe -server COMPUTER -user DOMAIN\username -password PASSWORD -command "ipconfig /all"
```

### Running Applications
```cmd
LiteRun.exe -server COMPUTER -exec "C:\Program Files\App\app.exe" -args "/silent"
```

### Interactive Session
```cmd
LiteRun.exe -server COMPUTER -interactive
```

## Command Line Options

```
LiteRun.exe [options]
  -server <hostname>     Target computer name or IP address
  -command <cmd>         Command to execute
  -exec <path>           Executable file to run
  -args <arguments>      Arguments for executable
  -user <username>       Authentication username
  -password <password>   Authentication password
  -timeout <seconds>     Operation timeout (default: 30)
  -interactive           Start interactive session
  -verbose               Enable verbose output
  -log <file>            Log output to file
```

## Configuration

### Environment Variables
- `LITERUN_DEFAULT_TIMEOUT`: Default timeout for operations
- `LITERUN_LOG_LEVEL`: Logging verbosity level
- `LITERUN_CONFIG_PATH`: Path to configuration file

### Configuration File
Create `literun.config` in the application directory for default settings:
```ini
[DEFAULT]
Timeout=60
LogLevel=INFO
RetryCount=3
```

## Technology Stack

- **Platform**: Windows (Native C++)
- **Networking**: Windows Networking APIs, WinRM
- **Security**: Windows Security APIs, SSPI
- **Communication**: Named Pipes, TCP/IP

## Development Status

This project is **Work in Progress** and **not actively developed**. Core functionality is operational but some advanced features may be incomplete. Moderate contributions are accepted for improvements and bug fixes.

## System Requirements

- Windows 7 or later (both local and remote)
- Network connectivity to target systems
- Appropriate permissions on remote systems
- Windows Remote Management (WinRM) enabled on targets

## Troubleshooting

### Common Issues
- **Connection Failed**: Check network connectivity and WinRM configuration
- **Access Denied**: Verify user permissions on remote system
- **Timeout Errors**: Increase timeout value or check network latency
- **Authentication Failed**: Verify credentials and authentication method

### WinRM Configuration
On target systems, ensure WinRM is properly configured:
```cmd
winrm quickconfig
winrm set winrm/config/service/auth @{Basic="true"}
```

## Security Considerations

- Use appropriate authentication methods for your environment
- Consider using service accounts with minimal required privileges
- Monitor and audit remote execution activities
- Ensure network communications are properly secured

## License

This software is released under [The Unlicense](LICENSE) - public domain.

---

*LiteRun provides efficient remote execution capabilities for Windows system administration and automation.*

## 🌟 Part of LiteSuite

This tool is part of **[LiteSuite](https://github.com/svtica/LiteSuite)** - a comprehensive collection of lightweight Windows administration tools.

### Other Tools in the Suite:
- **[LiteTask](https://github.com/svtica/LiteTask)** - Advanced Task Scheduler Alternative  
- **[LitePM](https://github.com/svtica/LitePM)** - Process Manager with System Monitoring
- **[LiteDeploy](https://github.com/svtica/LiteDeploy)** - Network Deployment and Management
- **[LiteRun](https://github.com/svtica/LiteRun)** - Remote Command Execution Utility
- **[LiteSrv](https://github.com/svtica/LiteSrv)** - Windows Service Wrapper

### 📦 Download the Complete Suite
Get all tools in one package: **[LiteSuite Releases](https://github.com/svtica/LiteSuite/releases/latest)**

---

*LiteSuite - Professional Windows administration tools for modern IT environments.*
