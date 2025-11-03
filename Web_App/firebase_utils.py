# firebase_utils.py

import requests
import pandas as pd
from typing import Optional, Dict, Any

# ====== Firebase Configuration ======
FIREBASE_URL = "https://htn-iot-8e223-default-rtdb.asia-southeast1.firebasedatabase.app"
FIREBASE_PATH_DATA = "/sensor/data"
FIREBASE_PATH_CONTROL = "/sensor/control"
FIREBASE_AUTH_TOKEN = "ok7JtCJv8A9JTdhfsNUZwkG8GyvOO1pkY5PAgn5e"

# ====== Firebase Helper Functions ======

def build_url(path: str) -> str:
    """Build full Firebase REST URL with authentication token."""
    return f"{FIREBASE_URL}{path}.json?auth={FIREBASE_AUTH_TOKEN}"


def get_device_data(device_id: str) -> Optional[Dict[str, Any]]:
    """Get raw JSON data for a specific device from Firebase."""
    url = build_url(f"{FIREBASE_PATH_DATA}/{device_id}")
    try:
        resp = requests.get(url, timeout=8)
        if resp.status_code == 200:
            return resp.json()
    except Exception:
        pass
    return None


def get_time_series_from_device(device_id: str, limit: int = 200) -> pd.DataFrame:
    """
    Convert Firebase sensor data structure to pandas DataFrame.
    Supports time-series nodes where keys are timestamps.
    """
    data = get_device_data(device_id)
    if not data or not isinstance(data, dict):
        return pd.DataFrame()

    rows = []
    for key, value in data.items():
        if isinstance(value, dict):
            try:
                ts = int(key)
                timestamp = pd.to_datetime(ts, unit="s")
            except Exception:
                timestamp = pd.Timestamp.now()
            row = value.copy()
            row["timestamp"] = timestamp
            rows.append(row)

    if not rows:
        # Fallback if device only reports a flat JSON
        df = pd.DataFrame([data])
        df["timestamp"] = pd.Timestamp.now()
        return df

    df = pd.DataFrame(rows)
    df = df.sort_values("timestamp").tail(limit)
    return df.reset_index(drop=True)


def get_latest_values(device_id: str) -> Dict[str, Any]:
    """Return the latest measurement values for a device."""
    df = get_time_series_from_device(device_id, limit=1)
    if df.empty:
        return {}
    return df.iloc[-1].to_dict()


def write_control_to_device(device_id: str, control_payload: Dict[str, Any]) -> bool:
    """
    Write control object to Firebase under /sensor/control/<device_id>.
    Example: {"mode": "auto", "target_humidity": 60}
    """
    url = build_url(f"{FIREBASE_PATH_CONTROL}/{device_id}")
    try:
        resp = requests.put(url, json=control_payload, timeout=8)
        return resp.status_code in (200, 201)
    except Exception:
        return False
