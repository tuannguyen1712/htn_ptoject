from dash import Dash, html, dcc
import dash_bootstrap_components as dbc
import pandas as pd

app = Dash(__name__, external_stylesheets=[dbc.themes.CYBORG])
app.title = "🌡️ IoT Realtime Dashboard"
server = app.server

# --- Layout Components ---
Mode_and_Humidity_Setpoint_Panel = \
dbc.Card([
    dbc.CardHeader("⚙️ Humidity Control", className="fw-bold small"),
    dbc.CardBody([
        dbc.Row([
            dbc.Col([
                html.Label("Target (%)", className="text-light small"),
                dbc.Input(id="humidity-setpoint", type="number", min=30, max=90, step=1, value=70, size="sm"),
            ], md=4),
            dbc.Col([
                html.Label("Sampling Interval", className="text-light small"),
                # dbc.Button("ON", id="state-btn", color="success", className="w-100 btn-sm")
                dbc.Input(id="sampling-interval", type="number", min=1, max=255, step=1, value=5, size="sm"),
            ], md=4),
            dbc.Col([
                html.Label("Mode", className="text-light small"),
                dbc.Button("AUTO", id="mode-btn", color="success", className="w-100 btn-sm")
            ], md=4),
        ], className="mb-2"),
        dbc.Button("Send", id="send-control-btn", color="primary", className="w-100 btn-sm"),
        html.Div(id="control-status", className="text-center mt-1 text-info small"),
    ])
], color="dark", inverse=True, style={"height": "100%"})

Wifi_Config_Panel = \
dbc.Card([
    dbc.CardBody([
        html.Div([
            dbc.Button("⚙️", id="toggle-wifi-btn",
                        color="warning", size="sm",
                        style={"height": "34px", "width": "34px", "marginRight": "5px"}),

            html.Div([
                dbc.Input(id="ssid-input", placeholder="SSID", size="sm",
                            style={"marginRight": "4px"}),
                dbc.Input(id="pass-input", placeholder="Password", type="password", size="sm",
                            style={"marginRight": "4px"}),
                dbc.Button("Send", id="send-wifi-btn",
                            color="primary", size="sm")
            ],
                id="wifi-panel",
                style={
                    "display": "flex",
                    "alignItems": "center",
                    "overflow": "hidden",
                    "whiteSpace": "nowrap",
                    "width": "0px",
                    "transition": "width 0.4s ease",
                }
            )
        ], style={
            "display": "flex",
            "alignItems": "center",
            "justifyContent": "flex-start",
            "height": "36px"
        }),
        html.Div(id="wifi-status", className="text-center mt-1 text-info small")
    ])
], color="dark", inverse=True, style={"height": "70px", "marginBottom": "10px"})

Current_Sensor_Values_Panel = \
html.Div([
    dbc.Row([
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("🌡️ Temp", className="text-center fw-bold small"),
                dbc.CardBody(
                    html.H5(id="temp-value", className="text-warning text-center mb-0"),
                    className="d-flex align-items-center justify-content-center",
                    style={"height": "100%"}
                )
            ], color="dark", inverse=True, style={"height": "100%"})
        ], width=4, style={"height": "100%"}),

        dbc.Col([
            dbc.Card([
                dbc.CardHeader("💧 Humidity", className="text-center fw-bold small"),
                dbc.CardBody(
                    html.H5(id="humi-value", className="text-info text-center mb-0"),
                    className="d-flex align-items-center justify-content-center",
                    style={"height": "100%"}
                )
            ], color="dark", inverse=True, style={"height": "100%"})
        ], width=4, style={"height": "100%"}),

        dbc.Col([
            dbc.Card([
                dbc.CardHeader("☁️ CO₂", className="text-center fw-bold small"),
                dbc.CardBody(
                    html.H5(id="co2-value", className="text-success text-center mb-0"),
                    className="d-flex align-items-center justify-content-center",
                    style={"height": "100%"}
                )
            ], color="dark", inverse=True, style={"height": "100%"})
        ], width=4, style={"height": "100%"}),
    ], className="g-1", style={"height": "100%"})
], style={
    "height": "120px",
    "marginBottom": "10px",
    "display": "flex",
    "flexDirection": "column",
})

# --- Time Range selector ---
Time_Range_Selector_Panel = \
dbc.Card([
    dbc.CardHeader("⏱️ Select Time Range", className="fw-bold small"),
    dbc.CardBody([
        dbc.Row([
            # --- Start Date & Time ---
            dbc.Col([
                html.Label("Start Date & Time", className="text-light small mb-1"),
                dbc.InputGroup([
                    dcc.DatePickerSingle(
                        id="start-date",
                        display_format="YYYY-MM-DD",
                        date=pd.Timestamp.now().date(),
                        style={"width": "100%"}
                    ),
                    dbc.Input(
                        id="start-time",
                        type="time",
                        value="00:00:01",
                        size="sm",
                        style={"maxWidth": "82%"}
                    )
                ], className="d-flex gap-1")
            ], width=5),

            # --- End Date & Time ---
            dbc.Col([
                html.Label("End Date & Time", className="text-light small mb-1"),
                dbc.InputGroup([
                    dcc.DatePickerSingle(
                        id="end-date",
                        display_format="YYYY-MM-DD",
                        date=pd.Timestamp.now().date(),
                        style={"width": "100%"}
                    ),
                    dbc.Input(
                        id="end-time",
                        type="time",
                        value="23:59:59",
                        size="sm",
                        style={"maxWidth": "82%"}
                    )
                ], className="d-flex gap-1")
            ], width=5),

            # --- Apply Button ---
            dbc.Col([
                dbc.Button("Apply 🔍", id="apply-btn", color="primary", className="w-65 btn-sm h-50"),
                dbc.Button("Back ⏪", id="back-btn", color="warning", className="w-65 btn-sm h-50")
            ], width=2)
        ])
    ], className="g-1"),
], color="dark", inverse=True, className="g-1", style={"height": "200px", "marginBottom": "10px"})

Real_Time_Graph_Panel = \
html.Div([
    dbc.Row([
        dbc.Col([
            dbc.Card([
                dbc.CardHeader("🌡️ Temperature (°C)", className="fw-bold small"),
                dbc.CardBody([dcc.Graph(id="temp-graph", config={"displayModeBar": False}, style={"height": "300px"})])
            ], color="dark", inverse=True)
        ], width=4),

        dbc.Col([
            dbc.Card([
                dbc.CardHeader("💧 Humidity (%)", className="fw-bold small"),
                dbc.CardBody([dcc.Graph(id="humi-graph", config={"displayModeBar": False}, style={"height": "300px"})])
            ], color="dark", inverse=True)
        ], width=4),

        dbc.Col([
            dbc.Card([
                dbc.CardHeader("☁️ CO₂ (ppm)", className="fw-bold small"),
                dbc.CardBody([dcc.Graph(id="co2-graph", config={"displayModeBar": False}, style={"height": "300px"})])
            ], color="dark", inverse=True)
        ], width=4),
    ], className="g-2"),  # g-2 for gutter spacing

    dcc.Interval(id="refresh-interval", interval=5000, n_intervals=0)
])

# --- Layout ---
app.layout = dbc.Container([
    # Header
    html.H4("💡 IoT Environmental Dashboard",
            className="text-center fw-bold my-2 text-white",
            style={"fontSize": "28px"}),

    # Toolbar Panels
    html.Div([
        # Mode and Humidity Setpoint Panel    
        html.Div([Mode_and_Humidity_Setpoint_Panel], style={"flex": "1", "maxWidth": "33%"}),
        # WiFi Config and Current Sensor Values Panel
        html.Div([
            Wifi_Config_Panel,
            Current_Sensor_Values_Panel,
        ], style={"flex": "1", "marginLeft": "10px", "maxWidth": "33%"}),
        # Time Range Selector Panel
        html.Div([Time_Range_Selector_Panel], style={"flex": "1", "marginLeft": "10px", "maxWidth": "34%"})
    ], style={"display": "flex", "height": "200px", "marginBottom": "10px"}),

    # Real-Time Graph Panel
    Real_Time_Graph_Panel
], fluid=True)
