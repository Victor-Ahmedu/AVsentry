"""
convert_model.py — compiles the trained Decision Tree into plain C
using micromlgen, producing model.h for the Arduino sketch.

This is what makes on-device inference possible on the ESP8266 without
a TensorFlow Lite Micro runtime: the "model" becomes ordinary nested
if-else statements, native and tiny.
"""

import joblib
from micromlgen import port

clf = joblib.load("trained_model.joblib")

c_code = port(clf, classmap={i: label for i, label in enumerate(clf.classes_)})

with open("model.h", "w") as f:
    f.write(c_code)

print("Saved model.h")
print("\n--- Preview ---")
print(c_code[:500])
print("\nReminder: check that model.h ends with the correct four closing")
print("braces for the Eloquent::ML::Port namespaces — micromlgen has been")
print("known to omit the final one. See firmware/model.h for details.")
