# app.py
from dash import Dash, html, dcc, Input, Output, State
import dash_bootstrap_components as dbc
import plotly.express as px
import pandas as pd
import datetime

from firebase_utils import get_time_series_from_device, get_latest_values, write_control_to_device
from wifi_config_utils import send_wifi_config_to_device

# ====== Default configuration ======
DEFAULT_DEVICE_ID = "ESP32_ABC123"

app = Dash(__name__, external_stylesheets=[dbc.themes.CYBORG])
app.title = "🌡️ IoT Realtime Dashboard"
server = app.server

# ====== Layout ======
app.layout = dbc.Container([
    html.H2("💡 IoT Environmental Dashboard", className="text-center my-4"),

    dbc.Row([
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("🌡️ Temperature"),
                dbc.CardBody([
                    html.H3(id="temp-value", className="text-warning text-center"),
                ])
            ], color="dark", inverse=True)
        ], md=4),
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("💧 Humidity"),
                dbc.CardBody([
                    html.H3(id="humi-value", className="text-info text-center"),
                ])
            ], color="dark", inverse=True)
        ], md=4),
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("☁️ CO₂"),
                dbc.CardBody([
                    html.H3(id="co2-value", className="text-success text-center"),
                ])
            ], color="dark", inverse=True)
        ], md=4),
    ], className="mb-4"),

    dbc.Row([
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("📈 Sensor data over time"),
                dbc.CardBody([
                    dcc.Graph(id="sensor-graph", config={"displayModeBar": False}),
                    dcc.Interval(id="refresh-interval", interval=5000, n_intervals=0)
                ])
            ])
        ], width=12)
    ], className="mb-4"),

    #Control section
    dbc.Row([
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("⚙️ Humidity Control"),
                dbc.CardBody([
                    dbc.Row([
                        dbc.Col([
                            html.Label("Set target humidity (%)"),
                            dbc.Input(id="humidity-setpoint", type="number", min=30, max=90, step=1, value=60),
                        ], md=6),
                        dbc.Col([
                            html.Label("Mode"),
                            dbc.Button("AUTO", id="mode-btn", color="success", className="w-100")
                        ], md=6),
                    ], className="mb-3"),
                    dbc.Button("Send Control", id="send-control-btn", color="primary", className="w-100"),
                    html.Div(id="control-status", className="text-center mt-3 text-info"),
                ])
            ])
        ], md=6),
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("📶 WiFi Configuration"),
                dbc.CardBody([
                    dbc.Input(id="ssid-input", placeholder="WiFi SSID", className="mb-2"),
                    dbc.Input(id="pass-input", placeholder="WiFi Password", type="password", className="mb-2"),
                    dbc.Button("Send WiFi Config", id="send-wifi-btn", color="warning", className="w-100"),
                    html.Div(id="wifi-status", className="text-center mt-3 text-info")
                ])
            ])
        ], md=6)
    ])
], fluid=True)

# ====== Callbacks ======

# --- Update sensor readings and graph ---
@app.callback(
    Output("sensor-graph", "figure"),
    Output("temp-value", "children"),
    Output("humi-value", "children"),
    Output("co2-value", "children"),
    Input("refresh-interval", "n_intervals"),
)
def update_sensor_data(n):
    df = get_time_series_from_device(DEFAULT_DEVICE_ID, limit=50)
    if df.empty:
        fig = px.line(title="No data available yet")
        return fig, "N/A", "N/A", "N/A"

    # Build line graph
    y_cols = [col for col in ["Temp", "Humi", "CO2"] if col in df.columns]
    fig = px.line(df, x="timestamp", y=y_cols, markers=True)
    fig.update_layout(
        template="plotly_dark",
        title="Sensor values over time",
        margin=dict(l=20, r=20, t=40, b=20),
        legend_title_text=""
    )

    latest = df.iloc[-1].to_dict()
    temp = f"{latest.get('Temp', 'N/A')} °C"
    humi = f"{latest.get('Humi', 'N/A')} %"
    co2 = f"{latest.get('CO2', 'N/A')} ppm"
    return fig, temp, humi, co2


# --- Toggle mode button (Auto/Manual) ---
@app.callback(
    Output("mode-btn", "children"),
    Output("mode-btn", "color"),
    Input("mode-btn", "n_clicks"),
    prevent_initial_call=True
)
def toggle_mode(n):
    if n % 2 == 1:
        return "MANUAL", "danger"
    return "AUTO", "success"


# --- Send humidity control to Firebase ---
@app.callback(
    Output("control-status", "children"),
    Input("send-control-btn", "n_clicks"),
    State("humidity-setpoint", "value"),
    State("mode-btn", "children"),
    prevent_initial_call=True
)
def send_control(n, setpoint, mode):
    payload = {
        "target_humidity": int(setpoint or 0),
        "mode": mode.lower(),
        "timestamp": int(datetime.datetime.now().timestamp())
    }
    ok = write_control_to_device(DEFAULT_DEVICE_ID, payload)
    if ok:
        return f"Sent control: {payload}"
    return "Failed to send control."


# --- Send WiFi config to device (SoftAP) ---
@app.callback(
    Output("wifi-status", "children"),
    Input("send-wifi-btn", "n_clicks"),
    State("ssid-input", "value"),
    State("pass-input", "value"),
    prevent_initial_call=True
)
def send_wifi(n, ssid, password):
    if not ssid or not password:
        return "SSID and password are required."
    ok, resp = send_wifi_config_to_device(ssid, password)
    if ok:
        return f"WiFi config sent successfully. Response: {resp}"
    else:
        return f"Failed to send config: {resp}"

# ====== Run server ======
if __name__ == "__main__":
    app.run(debug=True)
