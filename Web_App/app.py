from front_end import *
from dash import Input, Output, State, callback_context, no_update
import plotly.express as px
import datetime
import warnings

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
    Output("control-status", "children"),
    Input("send-control-btn", "n_clicks"),
    State("humidity-setpoint", "value"),
    State("sampling-interval", "value"),
    State("mode-btn", "children"),
    prevent_initial_call=True
)
def send_control(n, setpoint, interval, mode):
    mode = 1 if "AUTO" in mode else 0
    
    payload = {
        "mode": mode,
        "sampling_interval": interval,
        "humi_thres": setpoint
    }
    ok = write_control_to_device(DEFAULT_DEVICE_ID, payload)
    return f"(Sent) Mode: {payload['mode']} Sampling Interval: {payload['sampling_interval']} Huminity Threshold {payload['humi_thres']}" \
            if ok else "Failed to send control."

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

in_time_range_select_mode = False
@app.callback(
    Output("temp-graph", "figure"),
    Output("humi-graph", "figure"),
    Output("co2-graph", "figure"),
    Output("temp-value", "children"),
    Output("humi-value", "children"),
    Output("co2-value", "children"),
    Input("refresh-interval", "n_intervals"),
    Input("apply-btn", "n_clicks"),
    Input("back-btn", "n_clicks"),
    State("start-date", "date"),
    State("start-time", "value"),
    State("end-date", "date"),
    State("end-time", "value"),
)
def update_and_toggle_mode(n_intervals, apply_clicks, back_clicks, start_date, start_time, end_date, end_time):
    global in_time_range_select_mode
    ctx = callback_context
    triggered_id = ctx.triggered_id if ctx.triggered else None

    # --- Filter Mode ---
    if triggered_id == "apply-btn":
        if not (start_date and end_date):
            return no_update, no_update, no_update, no_update, no_update, no_update
        start_dt = pd.to_datetime(f"{start_date} {start_time}")
        end_dt = pd.to_datetime(f"{end_date} {end_time}")
        df = get_device_data_by_time(DEFAULT_DEVICE_ID, start_dt, end_dt)
        in_time_range_select_mode = True

    # --- Back to Realtime Mode ---
    elif triggered_id == "back-btn":
        today = datetime.date.today().strftime("%Y:%m:%d")
        df = get_data_series_from_device(DEFAULT_DEVICE_ID, today, limit=50)
        in_time_range_select_mode = False

    # --- Realtime auto-update ---
    elif triggered_id == "refresh-interval" and not in_time_range_select_mode:
        today = datetime.date.today().strftime("%Y:%m:%d")
        df = get_data_series_from_device(DEFAULT_DEVICE_ID, today, limit=50)
    else:
        return no_update, no_update, no_update, no_update, no_update, no_update

    # --- If no data ---
    if df.empty:
        empty_fig = px.line(title="No data available")
        empty_fig.update_layout(template="plotly_dark")
        return empty_fig, empty_fig, empty_fig, "N/A", "N/A", "N/A"

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

    latest = df.iloc[-1].to_dict()
    return fig_temp, fig_humi, fig_co2, \
        f"{latest.get('Temp', 0):.2f} °C", f"{latest.get('Humi', 0):.2f} %", f"{latest.get('CO2', 0):.0f} ppm"

if __name__ == "__main__":
    app.run(debug=True)