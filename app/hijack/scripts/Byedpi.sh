#!/system/bin/sh

ACTION="$1"
SCRIPT="/system/bin/ciadpi"
PIDFILE="/var/run/byedpi.pid"
LOG_FILE="/var/log/byedpi.log"

# Optimized DPI bypass parameters
ARGS="--transparent --daemon --pidfile $PIDFILE -i 192.168.8.1 -p 999 \
-Ku -a3 -An -Kt,h -s0 -o1 -d1 -r1+s -Ar -At -f-1 -As  -Q orig -m 3"

# Initialize logging
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

# Check if iptables rule exists
rule_exists() {
    iptables -t nat -C "$@" >/dev/null 2>&1
}

# Add iptables rules safely
add_rules() {
    log "Adding iptables rules"

    rule_exists PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 80 -j ACCEPT || {
        iptables -t nat -A PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 80 -j ACCEPT
        log "Added HTTP whitelist rule"
    }

    rule_exists PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 443 -j ACCEPT || {
        iptables -t nat -A PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 443 -j ACCEPT
        log "Added HTTPS whitelist rule"
    }

    rule_exists PREROUTING -i br0 -p tcp --dport 80 -j REDIRECT --to-port 999 || {
        iptables -t nat -A PREROUTING -i br0 -p tcp --dport 80 -j REDIRECT --to-port 999
        log "Added HTTP redirect rule"
    }

    rule_exists PREROUTING -i br0 -p tcp --dport 443 -j REDIRECT --to-port 999 || {
        iptables -t nat -A PREROUTING -i br0 -p tcp --dport 443 -j REDIRECT --to-port 999
        log "Added HTTPS redirect rule"
    }
}

# Remove iptables rules safely
remove_rules() {
    log "Removing iptables rules"

    iptables -t nat -D PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 80 -j ACCEPT 2>/dev/null && log "Removed HTTP whitelist rule"
    iptables -t nat -D PREROUTING -i br0 -p tcp -d 192.168.8.1 --dport 443 -j ACCEPT 2>/dev/null && log "Removed HTTPS whitelist rule"
    iptables -t nat -D PREROUTING -i br0 -p tcp --dport 80 -j REDIRECT --to-port 999 2>/dev/null && log "Removed HTTP redirect rule"
    iptables -t nat -D PREROUTING -i br0 -p tcp --dport 443 -j REDIRECT --to-port 999 2>/dev/null && log "Removed HTTPS redirect rule"
}

# Start ByeDPI
start_byedpi() {
    if [ -f "$PIDFILE" ]; then
        pid=$(cat "$PIDFILE")
        if kill -0 "$pid" 2>/dev/null; then
            log "Service already running (PID: $pid)"
            return 0
        else
            log "Stale PID file found. Removing."
            rm -f "$PIDFILE"
        fi
    fi

    [ -x "$SCRIPT" ] || {
        log "Executable not found or not executable: $SCRIPT"
        return 1
    }

    log "Starting ByeDPI service"
    $SCRIPT $ARGS >> "$LOG_FILE" 2>&1 &

    local timeout=5
    while [ $timeout -gt 0 ]; do
        if [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE") 2>/dev/null; then
            add_rules
            log "Service started successfully (PID: $(cat $PIDFILE))"
            return 0
        fi
        sleep 1
        timeout=$((timeout-1))
    done

    log "Service failed to start"
    return 1
}

# Stop ByeDPI
stop_byedpi() {
    if [ ! -f "$PIDFILE" ]; then
        log "No PID file found. Service not running?"
        return 0
    fi

    pid=$(cat "$PIDFILE")
    if ! kill -0 "$pid" 2>/dev/null; then
        log "PID $pid not running. Cleaning up."
        rm -f "$PIDFILE"
        remove_rules
        return 0
    fi

    log "Stopping service (PID: $pid)"
    kill -15 "$pid" 2>/dev/null

    local timeout=5
    while [ $timeout -gt 0 ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            rm -f "$PIDFILE"
            remove_rules
            log "Service stopped successfully"
            return 0
        fi
        sleep 1
        timeout=$((timeout-1))
    done

    log "Force killing PID $pid"
    kill -9 "$pid" 2>/dev/null
    rm -f "$PIDFILE"
    remove_rules
    return 1
}

# Restart ByeDPI
restart_byedpi() {
    stop_byedpi
    sleep 2
    start_byedpi
}

# Check if ByeDPI is running
is_byedpi_running() {
    [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE") 2>/dev/null
}

# Print service status
get_status() {
    if is_byedpi_running; then
        echo "item:<Enabled>:ENABLED"
        echo "item: Disable:DISABLE"
        echo "item: Restart:RESTART"
        echo "text:Running (PID: $(cat $PIDFILE))"
    else
        echo "item: Enable:ENABLE"
        echo "item:<Disabled>:DISABLED"
        echo "text:Stopped"
    fi
}

# === Huawei E5785 Menu Interface ===
case "$ACTION" in
    "")
        echo "text:ByeDPI DPI Bypass:"
        get_status
        ;;
    ENABLE)
        echo "text:Starting ByeDPI..."
        start_byedpi
        echo "text:ByeDPI Status:"
        get_status
        ;;
    DISABLE)
        echo "text:Stopping ByeDPI..."
        stop_byedpi
        echo "text:ByeDPI Status:"
        get_status
        ;;
    RESTART)
        echo "text:Restarting ByeDPI..."
        restart_byedpi
        echo "text:ByeDPI Status:"
        get_status
        ;;
    *)
        echo "text:Invalid option"
        echo "text:Usage: $0 {ENABLE|DISABLE|RESTART}"
        ;;
esac