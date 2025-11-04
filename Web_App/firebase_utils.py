# firebase_utils.py

import requests
import pandas as pd
from typing import Optional, Dict, Any
import datetime
from config import *

def build_url(path: str) -> str:
    return f"{FIREBASE_URL}{path}.json?auth={FIREBASE_AUTH_TOKEN}"

def get_device_data(device_id: str) -> Optional[Dict[str, Any]]:
    url = build_url(f"{FIREBASE_PATH_DATA}/{device_id}")
    try:
        resp = requests.get(url, timeout=8)
        if resp.status_code == 200:
            return resp.json()
    except Exception:
        pass
    return None

"""
Write control object to Firebase under /sensor/control/<device_id>.
Example: {"mode": "auto", "target_humidity": 60}
"""
def write_control_to_device(device_id: str, control_payload: Dict[str, Any]) -> bool:
    url = build_url(f"{FIREBASE_PATH_CONTROL}/{device_id}")
    try:
        resp = requests.put(url, json=control_payload, timeout=8)
        return resp.status_code in (200, 201)
    except Exception:
        return False

"""
Fetch time series data from Firebase for a given device ID.
Data format in Firebase:
{
    "20251103160639": {
        "CO2": 448,
        "HumThres": 50,
        "Humi": 83.19,
        "Mode": 0,
        "State": 0,
        "Temp": 23.9,
        "Timestamp": "2025:11:03:16:06:39"
    }
}
"""
def get_data_series_from_device(device_id: str, limit: int = 10000) -> pd.DataFrame:
    data = get_device_data(device_id)
    # print("avc")
    if not data or not isinstance(data, dict):
        return pd.DataFrame()

    rows = []
    for key, val in data.items():
        if not isinstance(val, dict):
            continue
        ts_str = val.get("Timestamp")
        try:
            # Convert "YYYY:MM:DD:HH:MM:SS" to standard timestamp
            timestamp = pd.to_datetime(ts_str, format="%Y:%m:%d:%H:%M:%S", errors="coerce")
        except Exception:
            timestamp = ""
        rows.append({
            "timestamp": timestamp,
            "Temp": val.get("Temp", 0),
            "Humi": val.get("Humi", 0),
            "CO2": val.get("CO2", 0)
        })

    df = pd.DataFrame(rows)
    if df.empty:
        return df
    return df.tail(limit).reset_index(drop=True)
    
def get_device_data_by_time(device_id: str, start_dt: datetime.datetime, end_dt: datetime.datetime) -> pd.DataFrame:
    """
    Fetch sensor data from Firebase for a device between start_dt and end_dt.
    Returns a DataFrame filtered by timestamp range.
    """
    # Get all data for device
    df = get_data_series_from_device(device_id)
    if df.empty or "timestamp" not in df.columns:
        return pd.DataFrame()

    for data in df["timestamp"]:
        print(f"Data timestamp: {data}")
    # Parse timestamp column with known format
    try:
        df["timestamp"] = pd.to_datetime(df["timestamp"], format="%Y:%m:%d:%H:%M:%S", errors="coerce")
    except Exception:
        # fallback if format mismatch
        df["timestamp"] = pd.to_datetime(df["timestamp"], errors="coerce")

    # Remove invalid timestamps
    df = df.dropna(subset=["timestamp"])

    # Filter by selected time range
    mask = (df["timestamp"] >= start_dt) & (df["timestamp"] <= end_dt)
    filtered_df = df.loc[mask].reset_index(drop=True)

    print(f"Filtered data from {start_dt} to {end_dt}, {len(filtered_df)} records found.")
    return filtered_df
