from front_end import *
from dash import Input, Output, State, callback_context, no_update
from dash.exceptions import PreventUpdate
import plotly.express as px
import datetime
import warnings
import time
import threading
import json
import requests
from sseclient import SSEClient
from firebase_utils import *

new_data_available = False
latest_control = {}
new_control_available = False
current_control_status = None

# global data for display
fig_temp = px.line(title="No data")
fig_humi = px.line(title="No data")
fig_co2 = px.line(title="No data")
latest = {}
df = pd.DataFrame()

""" Internal module imports """
from firebase_utils import *
from wifi_config_utils import *
from config import DEFAULT_DEVICE_ID

""" Turn off future warnings """
warnings.filterwarnings("ignore", category=FutureWarning)

""" Callbacks for Wifi Configure Panel to be displayed/hidden """
@app.callback(
    Output("wifi-panel", "style"),
    Input("toggle-wifi-btn", "n_clicks"),
    State("wifi-panel", "style"),
    prevent_initial_call=True
)
def toggle_wifi_panel(n, style):
    if style["width"] == "0px":
        style["width"] = "500px"
    else:
        style["width"] = "0px"
    return style

""" Callback for toggle mode button """
@app.callback(
    Output("mode-btn", "children"),
    Output("mode-btn", "color"),
    Input("mode-btn", "n_clicks"),
    prevent_initial_call=True
)
def toggle_mode(n_clicks):
    if n_clicks % 2 == 1:
        return "MANUAL", "danger"
    return "AUTO", "success"

""" Callback for sending control command to firebase """
@app.callback(
    Output("control-status", "children", allow_duplicate=True),
    Input("send-control-btn", "n_clicks"),
    State("humidity-setpoint", "value"),
    State("sampling-interval", "value"),
    State("mode-btn", "children"),
    prevent_initial_call=True
)
def update_control_status(n, setpoint, interval, mode):
    global current_control_status
    timestamp = int(time.time())
    mode = 1 if "AUTO" in mode else 0
    
    setpoint_display = setpoint
    mode_display = "Manual"
    if mode == 1: #auto
        setpoint = 0
        setpoint_display = "Synchronizing"
        mode_display = "Auto"
    payload = {
        "mode": mode,
        "sampling_interval": interval,
        "humidity_threshold": setpoint,
        "timestamp": timestamp
    }
    ok = write_control_to_device(DEFAULT_DEVICE_ID, payload)
    current_control_status = f"(Current Status) Mode: {mode_display} || Sampling Interval: {interval} || Huminity Threshold {setpoint_display}" \
            if ok else "Failed to send control."
    return current_control_status

""" Callback for sending WiFi configuration to device """
@app.callback(
    Output("wifi-status", "children"),
    Input("send-wifi-btn", "n_clicks"),
    State("ssid-input", "value"),
    State("pass-input", "value"),
    prevent_initial_call=True
)
def send_wifi(n, ssid, password):
    if not ssid or not password:
        return "SSID and password required."
    ok, resp = send_wifi_config_to_device(ssid, password)
    return f"WiFi OK: {resp}" if ok else f"Failed: {resp}"

def update_graph(df: pd.DataFrame):
    # --- If no data ---
    if df.empty:
        empty_fig = px.line(title="No data available")
        empty_fig.update_layout(template="plotly_dark")
        return empty_fig, empty_fig, empty_fig

    # --- Build plots ---
    fig_temp = px.scatter(df, x="timestamp", y="Temp")
    fig_temp.update_layout(template="plotly_dark", margin=dict(l=20, r=20, t=40, b=30),
                           xaxis_title="Time", yaxis_title="Temperature (°C)")

    fig_humi = px.scatter(df, x="timestamp", y="Humi")
    fig_humi.update_layout(template="plotly_dark", margin=dict(l=20, r=20, t=40, b=30),
                           xaxis_title="Time", yaxis_title="Humidity (%)")

    fig_co2 = px.scatter(df, x="timestamp", y="CO2")
    fig_co2.update_layout(template="plotly_dark", margin=dict(l=20, r=20, t=40, b=30),
                           xaxis_title="Time", yaxis_title="CO₂ (ppm)")
    
    return fig_temp, fig_humi, fig_co2

in_time_range_select_mode = False
@app.callback(
    Output("temp-graph", "figure"),
    Output("humi-graph", "figure"),
    Output("co2-graph", "figure"),
    Output("temp-value", "children"),
    Output("humi-value", "children"),
    Output("co2-value", "children"),
    Output("control-status", "children", allow_duplicate=True),
    Input("refresh-interval", "n_intervals"),
    Input("apply-btn", "n_clicks"),
    Input("back-btn", "n_clicks"),
    State("start-date", "date"),
    State("start-time", "value"),
    State("end-date", "date"),
    State("end-time", "value"),
    prevent_initial_call=True
)
def update_and_toggle_mode(n_intervals, apply_clicks, back_clicks, start_date, start_time, end_date, end_time):
    global in_time_range_select_mode
    global new_data_available
    global fig_temp, fig_humi, fig_co2, df, latest
    global current_control_status

    triggered_id = callback_context.triggered_id if callback_context.triggered else None

    # --- Filter Mode ---
    if triggered_id == "apply-btn":
        start_dt = pd.to_datetime(f"{start_date} {start_time}")
        end_dt = pd.to_datetime(f"{end_date} {end_time}")
        df_filted = get_device_data_by_time(DEFAULT_DEVICE_ID, start_dt, end_dt)
        in_time_range_select_mode = True
        
        fig_temp, fig_humi, fig_co2 = update_graph(df_filted)
    
        latest = df.iloc[-1].to_dict()
        if df.empty: # No data
            return fig_temp, fig_humi, fig_co2, "N/A", "N/A", "N/A", current_control_status
        else:
            return fig_temp, fig_humi, fig_co2, \
                f"{latest.get('Temp', 0):.2f} °C", f"{latest.get('Humi', 0):.2f} %", f"{latest.get('CO2', 0):.0f} ppm", current_control_status
    else:
        # Update graph only if having new data and not in "time range select mode"
        # Still update the lastest data
        if not df.empty:
            latest = df.iloc[-1].to_dict()
            if triggered_id == "refresh-interval":
                if new_data_available and (not in_time_range_select_mode):
                    new_data_available = False
                else:
                    return fig_temp, fig_humi, fig_co2, \
                        f"{latest.get('Temp', 0):.2f} °C", f"{latest.get('Humi', 0):.2f} %", f"{latest.get('CO2', 0):.0f} ppm", current_control_status
            
            if triggered_id == "back-btn":
                in_time_range_select_mode = False

            fig_temp, fig_humi, fig_co2 = update_graph(df)

            return fig_temp, fig_humi, fig_co2, \
                f"{latest.get('Temp', 0):.2f} °C", f"{latest.get('Humi', 0):.2f} %", f"{latest.get('CO2', 0):.0f} ppm", current_control_status
        else:
            return fig_temp, fig_humi, fig_co2, "N/A", "N/A", "N/A", current_control_status

def firebase_sensor_listener():
    global df, new_data_available

    today = datetime.date.today().strftime("%Y:%m:%d")
    url = build_url(f"{FIREBASE_PATH_DATA}/{DEFAULT_DEVICE_ID}/{today}")
    headers = {"Accept": "text/event-stream"}
    session = requests.Session()
    req = requests.Request("GET", url, headers=headers)
    prepped = session.prepare_request(req)
    response = session.send(prepped, stream=True)
    client = SSEClient(response)

    for event in client.events():
        if not event.data or event.data == "null":
            continue

        try:
            payload = json.loads(event.data)
            data = payload.get("data")
            if not data or not isinstance(data, dict):
                continue

            if "Timestamp" not in data:
                continue

            ts = pd.to_datetime(data["Timestamp"], format="%Y:%m:%d:%H:%M:%S", errors="coerce")
            new_row = pd.DataFrame([{
                "timestamp": ts,
                "Temp": data.get("Temp", 0),
                "Humi": data.get("Humi", 0),
                "CO2": data.get("CO2", 0),
                "Mode": data.get("Mode", None),
                "State": data.get("State", None),
                "HumThres": data.get("HumThres", None)
            }])

            df = pd.concat([df, new_row], ignore_index=True)
            print(f"New sensor data appended: {data}")
            new_data_available = True

        except Exception as e:
            print("Sensor listener error:", e)

def firebase_control_listener():
    global latest_control, new_control_available, current_control_status

    url = build_url(f"{FIREBASE_PATH_CONTROL}/{DEFAULT_DEVICE_ID}")
    headers = {"Accept": "text/event-stream"}

    session = requests.Session()
    req = requests.Request("GET", url, headers=headers)
    prepped = session.prepare_request(req)
    response = session.send(prepped, stream=True)
    client = SSEClient(response)

    print("Listening control config stream...")

    for event in client.events():
        if not event.data or event.data == "null":
            continue

        try:
            payload = json.loads(event.data)
            data = payload.get("data")
            if not data or not isinstance(data, dict):
                continue

            latest_control = data
            new_control_available = True

            mode = data.get("mode", 0)
            display_mode = "Auto" if mode == 1 else "Manual"

            humidity_threshold = data.get("humidity_threshold", 0)
            if humidity_threshold == 0:
                humidity_threshold = "Synchronizing"

            current_control_status = (
                f"(Current Status) Mode: {display_mode} || "
                f"Sampling Interval: {data.get('sampling_interval', 'N/A')} || "
                f"Humidity Threshold: {humidity_threshold}"
            )

            print("New control config:", current_control_status)

        except Exception as e:
            print("Control listener error:", e)

if __name__ == "__main__":
    try:
        today = datetime.date.today().strftime("%Y:%m:%d")
        df = get_data_series_from_device(DEFAULT_DEVICE_ID, today)
        print(df)
        new_data_available = True
        mode, sampling_interval, humidity_threshold = get_current_control_status(DEFAULT_DEVICE_ID)
        display_mode = "Manual"
        if mode == 1:
            display_mode = "Auto"
        current_control_status = f"(Current Status) Mode: {display_mode} || Sampling Interval: {sampling_interval} || Huminity Threshold {humidity_threshold}"

    except Exception as e:
        print("Error:", e)

    # thresh for listening data from firebase
    # threading.Thread(target=firebase_listener, daemon=True).start()
    threading.Thread(target=firebase_sensor_listener, daemon=True).start()
    threading.Thread(target=firebase_control_listener, daemon=True).start()
    app.run(debug=True, use_reloader=False)
