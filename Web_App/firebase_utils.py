# firebase_utils.py

import requests
import pandas as pd
from typing import Optional, Dict, Any
import datetime
from config import *
from typing import Optional, Dict, Any

def build_url(path: str) -> str:
    return f"{FIREBASE_URL}{path}.json?auth={FIREBASE_AUTH_TOKEN}"

""" Fetch data of a specific date (format "YYYY:MM:DD") from Firebase for given device. """
def get_device_data(device_id: str, date_str: str) -> Optional[Dict[str, Any]]:
    url = build_url(f"{FIREBASE_PATH_DATA}/{device_id}/{date_str}")
    try:
        resp = requests.get(url, timeout=8)
        if resp.status_code == 200:
            return resp.json()
        else:
            print(f"Error: HTTP {resp.status_code} from Firebase.")
    except Exception as e:
        print(f"Error fetching data from Firebase: {e}")
    return None

""" Write control object to Firebase under /sensor/control/<device_id>. """
def write_control_to_device(device_id: str, control_payload: Dict[str, Any]) -> bool:
    url = build_url(f"{FIREBASE_PATH_CONTROL}/{device_id}")
    try:
        resp = requests.put(url, json=control_payload, timeout=8)
        return resp.status_code in (200, 201)
    except Exception:
        return False

import pandas as pd

"""
Convert a day's Firebase data into a pandas DataFrame.

Args:
    device_id: "ESP32_ABC123"
    date_str: folder name of date, format "YYYY:MM:DD"
    limit: max number of records to return
"""
def get_data_series_from_device(device_id: str, date_str: str, limit: int = 500) -> pd.DataFrame:
    data = get_device_data(device_id, date_str)
    if not data or not isinstance(data, dict):
        return pd.DataFrame()

    rows = []
    for time_key, val in data.items():
        if not isinstance(val, dict):
            continue
        ts_str = val.get("Timestamp")
        try:
            timestamp = pd.to_datetime(ts_str, format="%Y:%m:%d:%H:%M:%S", errors="coerce")
        except Exception:
            timestamp = pd.NaT

        rows.append({
            "timestamp": timestamp,
            "Temp": val.get("Temp", 0),
            "Humi": val.get("Humi", 0),
            "CO2": val.get("CO2", 0),
            "Mode": val.get("Mode", None),
            "State": val.get("State", None),
            "HumThres": val.get("HumThres", None)
        })

    df = pd.DataFrame(rows)
    if df.empty:
        return df

    df = df.sort_values("timestamp").tail(limit).reset_index(drop=True)
    return df

""" Fetch sensor data from Firebase for a device between start_dt and end_dt. """
def get_device_data_by_time(device_id: str, start_dt: datetime.datetime, end_dt: datetime.datetime) -> pd.DataFrame:

    date_cursor = start_dt.date()
    all_rows = []

    while date_cursor <= end_dt.date():
        date_key = date_cursor.strftime("%Y:%m:%d")
        df_day = get_data_series_from_device(device_id, date_key)
        if not df_day.empty:
            all_rows.append(df_day)
        date_cursor += datetime.timedelta(days=1)

    if not all_rows:
        print("No data found in requested range.")
        return pd.DataFrame()

    df = pd.concat(all_rows, ignore_index=True)
    df = df.dropna(subset=["timestamp"])

    mask = (df["timestamp"] >= start_dt) & (df["timestamp"] <= end_dt)
    filtered_df = df.loc[mask].sort_values("timestamp").reset_index(drop=True)
    print(f"Fetched {len(filtered_df)} records from {start_dt} to {end_dt}")
    return filtered_df

def get_current_control_status(device_id):
    url = build_url(f"{FIREBASE_PATH_CONTROL}/{device_id}")
    try:
        response = requests.get(url, timeout=10)

        if response.status_code != 200:
            print(f"[ERROR] Firebase returned {response.status_code}: {response.text}")
            return None

        data = response.json()

        # Firebase return None if having no data
        if not data:
                print("[INFO] No data found at control path.")
                return "None", "None", "None"

        mode = data.get("mode")
        sampling_interval = data.get("sampling_interval")
        humidity_threshold = data.get("humidity_threshold")

        return mode, sampling_interval, humidity_threshold

    except Exception:
        return "Error", "Error", "Error"
