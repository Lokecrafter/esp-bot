import time
import threading
import queue
import sys
import subprocess

try:
    import pyttsx3
    pyttsx3_available = True
    print("[TTS] pyttsx3 import OK")
except Exception as e:
    pyttsx3_available = False
    print("[TTS] pyttsx3 import failed:", repr(e))

# On Windows, using PowerShell/System.Speech is often more reliable than pyttsx3
USE_POWERSHELL_TTS = sys.platform == "win32"

# Queue + worker thread so TTS calls are serialized and reliable
_tts_queue = queue.Queue()

def _tts_worker():
    if not pyttsx3_available:
        print("[TTS] worker: pyttsx3 not available, exiting")
        return
    try:
        engine = pyttsx3.init()
        engine.setProperty('rate', 160)
        print("[TTS] engine initialized in worker")
        try:
            voices = engine.getProperty('voices')
            print(f"[TTS] voices: {len(voices)} available")
            for i, v in enumerate(voices[:5]):
                print(f"[TTS] voice[{i}] id={getattr(v, 'id', None)} name={getattr(v, 'name', None)}")
        except Exception as e:
            print("[TTS] couldn't list voices:", e)
    except Exception as e:
        print("[TTS] engine init failed in worker:", repr(e))
        return

    while True:
        text = _tts_queue.get()
        if text is None:
            break
        try:
            print(f"[TTS] speaking: {text}")
            engine.say(text)
            engine.runAndWait()
            print(f"[TTS] done: {text}")
        except Exception as e:
            print("[TTS] error while speaking:", repr(e))
        _tts_queue.task_done()

if pyttsx3_available:
    _worker_thread = threading.Thread(target=_tts_worker, daemon=True)
    _worker_thread.start()

def speak(text: str):
    # If configured, use PowerShell SAPI TTS on Windows (synchronous, reliable)
    if USE_POWERSHELL_TTS:
        safe = text.replace('"', '\\"')
        cmd = [
            "powershell",
            "-Command",
            f"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak(\"{safe}\")",
        ]
        try:
            print(f"[TTS] powershell speaking: {text}")
            subprocess.run(cmd, check=True)
            print(f"[TTS] powershell done: {text}")
        except Exception as e:
            print("[TTS] powershell error:", repr(e))
        return

    if not pyttsx3_available:
        print(f"[TTS] not available, skipping speak: {text}")
        return

    print(f"[TTS] queueing: {text}")
    _tts_queue.put(text)

if __name__ == "__main__":
    try:
        while True:
            for word in ("Stövlar", "Katter"):
                print(word)
                speak(word)
                time.sleep(0.1)
    except KeyboardInterrupt:
        # If worker thread was started, signal it to exit and join
        if pyttsx3_available and '_worker_thread' in globals():
            _tts_queue.put(None)
            _worker_thread.join()