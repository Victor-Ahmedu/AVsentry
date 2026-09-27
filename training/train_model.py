"""
train_model.py — trains the Decision Tree used on-device.

Note on features: an earlier version included "hour of day" as a
feature (intended to distinguish night-time intrusion from daytime
presence). It was removed after discovering the model had only ever
seen a handful of training hours, so real-world hours it had never
seen during data collection caused false intrusion predictions. Using
only temp/humidity/ir removed that failure mode and the WiFi/NTP
dependency it required, at a small accuracy cost concentrated entirely
between the two heat-related classes (which is an acceptable trade-off
here — intrusion detection itself is unaffected).
"""

import pandas as pd
from sklearn.tree import DecisionTreeClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import joblib

df = pd.read_csv("sensor_log_clean.csv")

print("Total rows:", len(df))
print("\nRows per class:")
print(df["label"].value_counts())

X = df[["temp", "humidity", "ir"]]
y = df["label"]

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

# Kept shallow (max_depth=5) so the compiled C tree stays small enough
# for the ESP8266's limited flash/RAM.
clf = DecisionTreeClassifier(max_depth=5, random_state=42)
clf.fit(X_train, y_train)

y_pred = clf.predict(X_test)
print("\nClassification Report:")
print(classification_report(y_test, y_pred))

print("Confusion Matrix:")
print(confusion_matrix(y_test, y_pred, labels=clf.classes_))
print("Classes order:", clf.classes_)

joblib.dump(clf, "trained_model.joblib")
print("\nModel saved as trained_model.joblib")
