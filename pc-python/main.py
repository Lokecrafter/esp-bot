import asyncio
import websockets
import struct
import cv2
import numpy as np

PORT = 8765
ENCODER_STEPS = 128
EXPECTED_SIZE = (ENCODER_STEPS * 2) + (ENCODER_STEPS * 1) # 384 bytes

WINDOW_SIZE = 800
CENTER = WINDOW_SIZE // 2
MAX_DISTANCE_CM = 350  
SCALE = (WINDOW_SIZE / 2) / MAX_DISTANCE_CM 

def draw_empty_screen(status_text="Vantar pa ESP32..."):
    """Skapar en tom skärm med statusmeddelande."""
    img = np.zeros((WINDOW_SIZE, WINDOW_SIZE, 3), dtype=np.uint8)
    cv2.putText(img, status_text, (CENTER - 120, CENTER), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
    return img


async def send_led_data(websocket):
    # Skapa en lista med RGB-värden för 24 lysdioder
    # LED 1: Röd (255,0,0), LED 2: Grön (0,255,0), resten släckta...
    num_leds = 24
    led_data = bytearray(num_leds * 3)
    
    # Sätt första LED till helröd
    led_data[0] = 255 # R
    led_data[1] = 0   # G
    led_data[2] = 0   # B
    
    # Sätt andra LED till helgrön
    led_data[3] = 0   # R
    led_data[4] = 255 # G
    led_data[5] = 0   # B

    # print("Trying to send lidar data")
    # Skicka som binärdata över websocket
    await websocket.send(led_data)

async def handler(websocket):
    print(f"ESP32 ansluten från {websocket.remote_address}! Öppnar LiDAR-vy...")
    
    while True:
        try:
            message = await websocket.recv()
            
            if len(message) != EXPECTED_SIZE:
                # Om vi får ett trasigt paket vid omstart, hoppa bara över det
                # istället för att krascha eller låsa uppläsningen
                continue
            
            await send_led_data(websocket)
            
            format_str = f"<{ENCODER_STEPS}H{ENCODER_STEPS}B"
            unpacked = struct.unpack(format_str, message)
            
            distances = unpacked[0:ENCODER_STEPS]
            valid_flags = unpacked[ENCODER_STEPS:]
            
            img = np.zeros((WINDOW_SIZE, WINDOW_SIZE, 3), dtype=np.uint8)
            
            # Rita referensringar
            for r_cm in [100, 200, 300]:
                r_px = int(r_cm * SCALE)
                cv2.circle(img, (CENTER, CENTER), r_px, (40, 40, 40), 1)
            
            # Robotcentrum
            cv2.line(img, (CENTER - 8, CENTER), (CENTER + 8, CENTER), (0, 0, 255), 1)
            cv2.line(img, (CENTER, CENTER - 8), (CENTER, CENTER + 8), (0, 0, 255), 1)
            
            valid_count = 0
            for i in range(ENCODER_STEPS):
                if valid_flags[i] == 1:
                    dist = distances[i]
                    if dist < 10 or dist > MAX_DISTANCE_CM:
                        continue
                    
                    valid_count += 1
                    angle_deg = i * (360.0 / ENCODER_STEPS)
                    angle_rad = np.radians(angle_deg)
                    
                    # Spegelvänd och rättvänd (testa med minus framför sin)
                    x = int(CENTER - dist * SCALE * np.cos(angle_rad))
                    y = int(CENTER - dist * SCALE * np.sin(angle_rad))
                    
                    if 0 <= x < WINDOW_SIZE and 0 <= y < WINDOW_SIZE:
                        cv2.circle(img, (x, y), 3, (0, 255, 0), -1)
            
            cv2.putText(img, f"Punkter: {valid_count}/128", (15, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            
            cv2.imshow("ESP-Bot Realtime LiDAR", img)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
                
        except websockets.ConnectionClosed:
            print("ESP32 kopplade ifrån. Rensar skärmen...")
            cv2.imshow("ESP-Bot Realtime LiDAR", draw_empty_screen("ESP32 bortkopplad"))
            cv2.waitKey(1)
            break
        except Exception as e:
            print(f"Fel i strömmen: {e}")
            break



async def main():
    # Visa en tom startskärm direkt när servern drar igång
    cv2.imshow("ESP-Bot Realtime LiDAR", draw_empty_screen())
    cv2.waitKey(1)

    while True:
        try:
            # Denna loop gör att servern fortsätter leva även om klienten dör
            async with websockets.serve(handler, "0.0.0.0", PORT, ping_interval=None):
                print(f"Server startad på port {PORT}. Väntar på ESP32...")
                await asyncio.Future() # Kör tills servern stoppas manuellt
        except Exception as e:
            print(f"Serverfel, startar om lyssnaren: {e}")
            await asyncio.sleep(1)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nProgrammet avslutat av användaren.")
    finally:
        cv2.destroyAllWindows()