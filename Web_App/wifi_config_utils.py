# wifi_config_utils.py
# English comments only inside code as requested.

import requests
from typing import Tuple

def send_wifi_config_to_device(
    ssid: str,
    password: str,
    host: str = "192.168.4.1",
    port: int = 80,
    endpoint: str = "/wifi_config"
) -> Tuple[bool, str]:
    """
    Send WiFi configuration to the ESP32 device via HTTP POST request.
    Used when the device is in SoftAP mode.

    Args:
        ssid (str): WiFi SSID to connect to.
        password (str): WiFi password.
        host (str): Device IP (default "192.168.4.1").
        port (int): Device HTTP port (default 80).
        endpoint (str): API endpoint (default "/wifi_config").

    Returns:
        Tuple[bool, str]: (success, response_text_or_error)
    """
    url = f"http://{host}:{port}{endpoint}"
    payload = {"ssid": ssid, "pass": password}

    try:
        response = requests.post(url, json=payload, timeout=8)
        if response.status_code in (200, 201):
            return True, response.text
        else:
            return False, f"HTTP {response.status_code}: {response.text}"
    except Exception as e:
        return False, str(e)
