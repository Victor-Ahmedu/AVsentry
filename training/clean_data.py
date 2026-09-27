"""
clean_data.py — removes a real labeling bug found during development.

During data collection, the IR sensor was deliberately toggled on/off
throughout each "intrusion" and "occupied_uncomfortable" session (to
simulate intermittent motion), but the whole session kept one fixed
label. That left some rows labeled "intrusion" where the IR sensor was
actually reading clear (ir=1) at that exact moment — teaching the model
an incorrect association and causing false intrusion alarms on
perfectly normal or humid conditions.

This script removes exactly those contaminated rows before training.
"""

import pandas as pd

df = pd.read_csv("sensor_log.csv")

print("Before cleaning:", len(df), "rows")

mask = ~(
    ((df["label"] == "intrusion") & (df["ir"] == 1)) |
    ((df["label"] == "occupied_uncomfortable") & (df["ir"] == 1))
)
df_clean = df[mask]

print("After cleaning:", len(df_clean), "rows")
df_clean.to_csv("sensor_log_clean.csv", index=False)
print("Saved as sensor_log_clean.csv")
