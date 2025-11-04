# wifi_config_utils.py

import requests
from typing import Tuple
from config import *

def send_wifi_config_to_device(
    ssid: str,
    password: str,
    host: str = DEFAULT_HOST,
    port: int = DEFAULT_PORT,
    endpoint: str = DEFAULT_ENDPOINT
) -> Tuple[bool, str]:
    """
    Send WiFi configuration to the ESP32 device via HTTP POST request (SoftAP mode).

    Args:
        ssid: WiFi SSID to connect to.
        password: WiFi password.
        host: IoT device IP address.
        port: IoT device port.
        endpoint: API endpoint on the device.

    Returns:
        Tuple[success: bool, response_text_or_error: str]
    """
    url = f"http://{host}:{port}{endpoint}"
    payload = {SSID_KEY: ssid, PASSWORD_KEY: password}

    try:
        response = requests.post(url, json=payload, timeout=8)
        if response.status_code in (200, 201):
            return True, response.text
        else:
            return False, f"HTTP {response.status_code}: {response.text}"
    except Exception as e:
        return False, str(e)
