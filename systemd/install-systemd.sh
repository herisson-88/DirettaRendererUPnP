#!/bin/bash
# Diretta UPnP Renderer - Systemd Installation Script
# 
# This script installs the renderer as a systemd service
# Based on Piero's (AudioLinux) recommendations

set -e

INSTALL_DIR="/opt/diretta-renderer-upnp"
SERVICE_FILE="/etc/systemd/system/diretta-renderer.service"
CONFIG_FILE="$INSTALL_DIR/diretta-renderer.conf"

echo "════════════════════════════════════════════════════════"
echo "  Diretta UPnP Renderer - Systemd Service Installation"
echo "════════════════════════════════════════════════════════"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ Please run as root (sudo ./install-systemd.sh)"
    exit 1
fi

# Detect script location and find binary
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BINARY_PATH="$PROJECT_ROOT/bin/DirettaRendererUPnP"

echo "📂 Script location: $SCRIPT_DIR"
echo "📂 Project root:    $PROJECT_ROOT"
echo "📂 Looking for binary at: $BINARY_PATH"
echo ""

# Check if binary exists
if [ ! -f "$BINARY_PATH" ]; then
    echo "❌ Binary not found at: $BINARY_PATH"
    echo ""
    echo "Please ensure you have built the renderer:"
    echo "  cd $PROJECT_ROOT"
    echo "  make"
    echo ""
    echo "Or run this script from the project root:"
    echo "  cd $PROJECT_ROOT"
    echo "  sudo ./systemd/install-systemd.sh"
    exit 1
fi

echo "✓ Binary found: $BINARY_PATH"
echo ""

echo "1. Creating installation directory..."
mkdir -p "$INSTALL_DIR"

echo "2. Copying binary..."
cp "$BINARY_PATH" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/DirettaRendererUPnP"
echo "   ✓ Binary copied to $INSTALL_DIR/DirettaRendererUPnP"

echo "3. Creating default configuration file..."
if [ ! -f "$CONFIG_FILE" ]; then
    cat > "$CONFIG_FILE" << 'EOF'
# Diretta UPnP Renderer Configuration
# 
# This file is sourced by the systemd service unit.
# Modify these values to change renderer behavior.
#
# After changing values, reload:
#   sudo systemctl daemon-reload
#   sudo systemctl restart diretta-renderer

# Target Diretta device number (1 = first found)
TARGET=1

# UPnP port (default: 4005)
PORT=4005

# Buffer size in seconds (1.0 - 5.0)
# Recommended: 2.0 for most, 3.0-4.0 for DSD512+
BUFFER=2.0
EOF
    echo "   ✓ Configuration file created: $CONFIG_FILE"
else
    echo "   ℹ Configuration file already exists, keeping current settings"
fi

echo "4. Creating systemd service file..."
cat > "$SERVICE_FILE" << 'EOF'
[Unit]
Description=Diretta UPnP Renderer
Documentation=https://github.com/cometdom/DirettaRendererUPnP
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/diretta-renderer-upnp
EnvironmentFile=-/opt/diretta-renderer-upnp/diretta-renderer.conf
ExecStart=/opt/diretta-renderer-upnp/DirettaRendererUPnP --target ${TARGET} --port ${PORT} --buffer ${BUFFER}
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=diretta-renderer
AmbientCapabilities=CAP_NET_RAW CAP_NET_ADMIN

[Install]
WantedBy=multi-user.target
EOF

echo "5. Reloading systemd daemon..."
systemctl daemon-reload

echo "6. Enabling service (start on boot)..."
systemctl enable diretta-renderer.service

echo ""
echo "════════════════════════════════════════════════════════"
echo "  ✓ Installation Complete!"
echo "════════════════════════════════════════════════════════"
echo ""
echo "📝 Configuration file: $CONFIG_FILE"
echo "📝 Service file:       $SERVICE_FILE"
echo "📁 Installation dir:   $INSTALL_DIR"
echo ""
echo "🎯 Next steps:"
echo ""
echo "  1. Edit configuration (optional):"
echo "     sudo nano $CONFIG_FILE"
echo ""
echo "  2. Reload daemon:"
echo "     sudo systemctl daemon-reload"
echo ""  
echo "  3. Enable the service:"
echo "     sudo systemctl enable diretta-renderer"
echo ""
echo "  4. Start the service:"  
echo "     sudo systemctl start diretta-renderer"
echo ""
echo "  5. Check status:"
echo "     sudo systemctl status diretta-renderer"
echo ""
echo "  6. View logs:"
echo "     sudo journalctl -u diretta-renderer -f"
echo ""
echo "  7. Stop the service:"
echo "     sudo systemctl stop diretta-renderer"
echo ""
echo "  8. Disable auto-start:"
echo "     sudo systemctl disable diretta-renderer"
echo ""
echo "════════════════════════════════════════════════════════"
