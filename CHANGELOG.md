# Changelog

All notable changes to DirettaRendererUPnP will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-12-24

### Added
- 🌐 **Multi-interface support** for multi-homed systems
  - New command-line option: `--interface <name>` to bind to specific network interface (e.g., eth0, eno1, enp6s0)
  - New command-line option: `--bind-ip <address>` to bind to specific IP address (e.g., 192.168.1.10)
  - Essential for 3-tier architecture configurations with separate control and audio networks
  - Fixes SSDP discovery issues on systems with multiple network interfaces (VPN, multiple NICs, bridged networks)
  - Auto-detection remains default behavior for single-interface systems (backward compatible)

### Fixed
- **Critical**: Fixed format change freeze when transitioning between bit depths
  - **Issue**: Playlist playback would freeze for 10 seconds when switching between 24-bit and 16-bit tracks
  - **Root cause**: 4 residual samples in Diretta SDK buffer never drained, causing timeout
  - **Solution**: Implemented force flush with silence padding to push incomplete frames through pipeline
  - **Result**: Format changes now complete in ~200-300ms instead of 10-second timeout
  - **Impact**: Smooth playlist playback with mixed formats (16-bit/24-bit/32-bit)
- Improved error recovery during format transitions
- Better handling of incomplete audio frames at track boundaries

### Changed
- **UPnP Initialization**: Now uses `UpnpInit2()` with interface parameter for precise network binding
- **Format Change Timeout**: Reduced from 10s to 3s for faster error recovery
- **Buffer Drain Logic**: Added tolerance for ≤4 residual samples (considered "empty enough")
- **Hardware Stabilization**: Increased from 200ms to 300ms for better reliability during format changes
- **Logging**: Enhanced debug output during format change sequence with flush detection

### Configuration
- **Systemd**: New `NETWORK_INTERFACE` parameter in `/opt/diretta-renderer-upnp/diretta-renderer.conf`
  ```bash
  # For 3-tier architecture
  NETWORK_INTERFACE="eth0"      # Interface connected to control points
  
  # Or by IP address
  NETWORK_INTERFACE="192.168.1.10"
  ```
- **Wrapper Script**: Automatically detects whether parameter is IP address or interface name

### Use Cases

#### Multi-Interface Scenarios
1. **3-tier Architecture** (recommended by dsnyder):
   - Control Points (JPlay, Roon) on 192.168.1.x via eth0
   - Diretta DAC on 192.168.2.x via eth1
   ```bash
   sudo ./bin/DirettaRendererUPnP --interface eth0 --target 1
   ```

2. **VPN + Local Network**:
   - Local network on 192.168.1.x via eth0
   - VPN on 10.0.0.x via tun0
   ```bash
   sudo ./bin/DirettaRendererUPnP --bind-ip 192.168.1.10 --target 1
   ```

3. **Multiple Ethernet Adapters**:
   - Specify which adapter handles UPnP discovery
   ```bash
   sudo ./bin/DirettaRendererUPnP --interface eno1 --target 1
   ```

#### Format Change Improvements
- **Mixed Format Playlists**: Seamless transitions between 16-bit, 24-bit, and different sample rates
- **Streaming Services**: Better compatibility with services like Qobuz that mix bit depths
- **Gapless Playback**: Maintains gapless behavior even during format changes

### Documentation
- Added comprehensive **Multi-Homed Systems** section in README
- Added troubleshooting guide for network interface selection
- Added examples for common multi-interface configurations
- Updated systemd configuration guide
- Added FORMAT_CHANGE_FIX.md technical documentation

### Technical Details

#### Multi-Interface Implementation
- Modified `UPnPDevice.cpp`: `UpnpInit2(interfaceName, port)` instead of `UpnpInit2(nullptr, port)`
- Added `networkInterface` parameter to `UPnPDevice::Config` structure
- Propagated interface selection from command-line → DirettaRenderer → UPnPDevice
- Enhanced error messages when binding fails (suggests `ip link show`, permissions check)

#### Format Change Fix Implementation
- Added **Step 1.5** in `changeFormat()`: Force flush with 128 samples of silence padding
  - Pushes incomplete frames through Diretta SDK pipeline
  - Only triggered when residual < 64 samples detected
- Modified drain logic to accept small residual (≤4 samples) as successful
- Implemented `sendAudio()` function for unified audio data transmission
- Better synchronization between AudioEngine and DirettaOutput during transitions

### Breaking Changes
None - all changes are backward compatible

### Migration Guide
No migration needed. Existing configurations continue to work:
- Systems with single network interface: No changes required
- Multi-interface systems: Add `--interface` or configure `NETWORK_INTERFACE` in systemd

### Known Issues
- None reported

### Tested Configurations
- ✅ Fedora 39/40 (x64)
- ✅ Ubuntu 22.04/24.04 (x64)
- ✅ AudioLinux (x64)
- ✅ Raspberry Pi 4 (aarch64)
- ✅ 3-tier architecture with Intel i225 + RTL8125 NICs
- ✅ Mixed format playlists (16/24-bit, 44.1/96/192kHz)
- ✅ Qobuz streaming (16/24-bit)
- ✅ Local FLAC/WAV files
- ✅ DSD64/128/256 playback

### Performance
- Format change latency: ~200-300ms (down from 10s)
- Network discovery: Immediate on specified interface
- Memory usage: Unchanged
- CPU usage: Unchanged

### Credits
- Multi-interface support requested and tested by community members
- Format change fix developed in collaboration with Yu Harada (Diretta protocol creator)
- Testing and validation by early adopters on AudioPhile Style forum

---

## [1.0.8] - 2025-12-23

### Fixed
- Fixed SEEK functionality deadlock issue
  - Replaced mutex-based synchronization with atomic flag
  - Implemented asynchronous seek mechanism
  - Seek now completes in <100ms without blocking

### Changed
- Improved seek reliability and responsiveness
- Better error handling during seek operations

## [1.0.7] - 2025-12-22

### Added
- Advanced Diretta SDK configuration options:
  - `--thread-mode <value>`: Configure thread priority and behavior (bitmask)
  - `--cycle-time <µs>`: Transfer packet cycle maximum time (default: 10000)
  - `--cycle-min-time <µs>`: Transfer packet cycle minimum time (default: 333)
  - `--info-cycle <µs>`: Information packet cycle time (default: 5000)
  - `--mtu <bytes>`: Override MTU for network packets (default: auto-detect)

### Changed
- Buffer size parameter changed from integer to float for finer control
  - Now accepts values like `--buffer 2.5` for 2.5 seconds
- Improved buffer adaptation logic based on audio format complexity
- Better MTU detection and configuration

### Documentation
- Added comprehensive documentation for advanced Diretta SDK parameters
- Added thread mode bitmask reference
- Added MTU optimization guide

## [1.0.6] - 2025-12-21

### Fixed
- Audirvana Studio streaming compatibility issues
  - Fixed pink noise after 6-7 seconds when streaming 24-bit content from Qobuz
  - Issue was related to HTTP streaming implementation vs Diretta SDK buffer handling
  - Workaround: Use 16-bit or 20-bit streaming settings in Audirvana

### Changed
- Improved buffer handling for HTTP streaming sources
- Better error detection and recovery for streaming issues

## [1.0.5] - 2025-12-20

### Fixed
- Format change handling improvements
  - Fixed clicking sounds during 24-bit audio playback
  - Removed artificial silence generation that caused artifacts
  - Proper buffer draining using Diretta SDK's `buffer_empty()` methods

### Changed
- Improved audio playback behavior during track transitions
- Better handling of sample rate changes

## [1.0.4] - 2025-12-19

### Added
- Jumbo frame support with 16k MTU optimization
- Configurable MTU settings for network optimization

### Fixed
- Network configuration issues with Intel i225 cards (limited to 9k MTU)
- Buffer handling improvements

## [1.0.3] - 2025-12-18

### Added
- Gapless playback support
- Improved track transition handling

### Changed
- Better buffer management during track changes
- Improved format detection and handling

## [1.0.2] - 2025-12-17

### Fixed
- DSD playback improvements
- Sample rate detection accuracy

## [1.0.1] - 2025-12-16

### Added
- Support for multiple Diretta DAC targets
- Interactive target selection
- Command-line target specification

### Fixed
- Target discovery reliability
- Connection stability improvements

## [1.0.0] - 2025-12-15

### Added
- Initial release
- UPnP MediaRenderer implementation
- Diretta protocol integration
- Support for PCM audio (16/24/32-bit, up to 768kHz)
- Support for DSD audio (DSD64/128/256/512/1024)
- AVTransport service (Play, Pause, Stop, Seek, Next, Previous)
- RenderingControl service (Volume, Mute)
- ConnectionManager service
- Automatic SSDP discovery
- Format-specific buffer optimization
- Systemd service integration

### Supported Formats
- PCM: 16/24/32-bit, 44.1kHz to 768kHz
- DSD: DSD64, DSD128, DSD256, DSD512, DSD1024
- Containers: FLAC, WAV, AIFF, ALAC, APE, DSF, DFF

### Supported Control Points
- JPlay
- Roon
- BubbleUPnP
- Any UPnP/DLNA control point

---

## Version Numbering

- **Major.Minor.Patch** (e.g., 1.1.0)
- **Major**: Breaking changes or complete rewrites
- **Minor**: New features, significant improvements, backward compatible
- **Patch**: Bug fixes, minor improvements, backward compatible

## Unreleased

### Planned Features
- Web UI for configuration
- Docker container support
- Automatic format detection improvement
- Multi-room synchronization
- Volume normalization
- Equalizer support
