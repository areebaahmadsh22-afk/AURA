from flask import Flask, jsonify, request
import cv2
from deepface import DeepFace
import requests
import numpy as np
import os
from datetime import datetime

app = Flask(__name__)

# =========================
# Camera snapshot URL
# =========================
CAMERA_SNAPSHOT_URL = "http://194.210.157.215/capture"

# =========================
# Firebase Realtime Database
# IMPORTANT: no trailing slash
# =========================
FIREBASE_DB_URL = "https://iot-vibes-default-rtdb.europe-west1.firebasedatabase.app"

# Folder where images will be saved
UPLOAD_FOLDER = "uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

room_state = {
    "emotion": "neutral",
    "pulse": 75,
    "fan": "OFF",
    "ledColor": "#FFFFFF",
    "buzzer": False
}


def generate_filename(prefix="photo", ext=".jpg"):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{prefix}_{timestamp}{ext}"


def save_frame(frame, prefix="photo"):
    filename = generate_filename(prefix=prefix, ext=".jpg")
    filepath = os.path.join(UPLOAD_FOLDER, filename)

    success = cv2.imwrite(filepath, frame)
    if not success:
        raise RuntimeError("Failed to save image with cv2.imwrite")

    return filename, filepath


def save_raw_bytes(image_bytes, prefix="photo"):
    filename = generate_filename(prefix=prefix, ext=".jpg")
    filepath = os.path.join(UPLOAD_FOLDER, filename)

    with open(filepath, "wb") as f:
        f.write(image_bytes)

    return filename, filepath


def update_firebase(data):
    """
    Updates root-level keys in Firebase Realtime Database.
    Uses PATCH so only the provided keys are changed.
    """
    try:
        url = f"{FIREBASE_DB_URL}/.json"
        response = requests.patch(url, json=data, timeout=10)
        response.raise_for_status()
        print("Firebase updated successfully:", data)
        return True
    except Exception as e:
        print("Firebase update error:", e)
        return False


def reset_take_photo_flag():
    """
    Sets takePhoto back to false after processing.
    """
    try:
        url = f"{FIREBASE_DB_URL}/takePhoto.json"
        response = requests.put(url, json=False, timeout=10)
        response.raise_for_status()
        print("takePhoto reset to false")
        return True
    except Exception as e:
        print("Failed to reset takePhoto:", e)
        return False


def sync_room_state_to_firebase():
    """
    Sends the current room_state to Firebase root.
    """
    firebase_payload = {
        "emotion": room_state["emotion"].capitalize(),
        "bpm": room_state["pulse"],
        "buzzer": room_state["buzzer"],
        "ledColor": room_state["ledColor"],
        "song": "Midnight Serenade",
        "artist": "Luna Mist"
    }

    return update_firebase(firebase_payload)


def analyze_frame(frame):
    global room_state

    try:
        result = DeepFace.analyze(
            frame,
            actions=['emotion'],
            enforce_detection=False
        )

        if isinstance(result, list):
            emotion = result[0].get("dominant_emotion", "neutral")
        else:
            emotion = result.get("dominant_emotion", "neutral")

        print(f"Detected emotion: {emotion}")

        # You can later replace pulse with real sensor data
        pulse = room_state.get("pulse", 75)

        if emotion == 'sad':
            room_state.update({
                "emotion": "sad",
                "pulse": pulse,
                "fan": "ON",
                "ledColor": "#0000FF",
                "buzzer": True
            })
        elif emotion == 'happy':
            room_state.update({
                "emotion": "happy",
                "pulse": pulse,
                "fan": "OFF",
                "ledColor": "#FFD700",
                "buzzer": True
            })
        elif emotion == 'angry':
            room_state.update({
                "emotion": "angry",
                "pulse": pulse,
                "fan": "ON",
                "ledColor": "#FF0000",
                "buzzer": True
            })
        elif emotion == 'fear':
            room_state.update({
                "emotion": "fear",
                "pulse": pulse,
                "fan": "OFF",
                "ledColor": "#800080",
                "buzzer": True
            })
        else:
            room_state.update({
                "emotion": emotion,
                "pulse": pulse,
                "fan": "OFF",
                "ledColor": "#FFFFFF",
                "buzzer": False
            })

        # Push result to Firebase after analysis
        sync_room_state_to_firebase()

    except Exception as e:
        print("DeepFace analyze error:", e)
        room_state.update({
            "emotion": "neutral",
            "pulse": 75,
            "fan": "OFF",
            "ledColor": "#FFFFFF",
            "buzzer": False
        })

        sync_room_state_to_firebase()


@app.route('/status', methods=['GET'])
def get_status():
    return jsonify(room_state)


@app.route('/capture', methods=['POST', 'GET'])
def capture_image():
    """
    Server fetches one snapshot from the camera URL,
    saves it to uploads/, analyzes it, writes result to Firebase,
    and resets takePhoto to false.
    """
    try:
        response = requests.get(CAMERA_SNAPSHOT_URL, timeout=10)
        response.raise_for_status()

        img_array = np.frombuffer(response.content, np.uint8)
        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        if frame is None:
            return jsonify({
                "success": False,
                "error": "Could not decode image from camera snapshot"
            }), 500

        filename, filepath = save_frame(frame, prefix="capture")
        analyze_frame(frame)
        reset_take_photo_flag()

        return jsonify({
            "success": True,
            "message": "Image captured, saved, analyzed, and synced to Firebase",
            "filename": filename,
            "saved_to": filepath,
            "room_state": room_state
        })

    except Exception as e:
        print("Capture error:", e)
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/upload', methods=['POST'])
def upload_image():
    """
    Receives raw JPEG bytes from ESP32-CAM,
    saves them to uploads/, analyzes them, writes result to Firebase,
    and resets takePhoto to false.
    """
    try:
        image_bytes = request.data

        if not image_bytes:
            return jsonify({
                "success": False,
                "error": "No image data received"
            }), 400

        filename, filepath = save_raw_bytes(image_bytes, prefix="upload")

        img_array = np.frombuffer(image_bytes, np.uint8)
        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        if frame is None:
            return jsonify({
                "success": False,
                "error": "Image was saved, but could not be decoded"
            }), 500

        analyze_frame(frame)
        reset_take_photo_flag()

        return jsonify({
            "success": True,
            "message": "Image uploaded, saved, analyzed, and synced to Firebase",
            "filename": filename,
            "saved_to": filepath,
            "room_state": room_state
        })

    except Exception as e:
        print("Upload error:", e)
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)