# import asyncio
# import websockets
# import struct
# import csv

# PORT = 8765
# ENCODER_STEPS = 128
# # 128 stycken uint16 (H) och 128 stycken uint8 (B) = 256 + 128 = 384 bytes
# EXPECTED_SIZE = (ENCODER_STEPS * 2) + (ENCODER_STEPS * 1) 
# FILENAME = "lidar_data.csv"

# # Struct-format: '<' för little-endian, '128H' för distanser, '128B' för valid-flaggor
# STRUCT_FORMAT = f"<{ENCODER_STEPS}H{ENCODER_STEPS}B"

# async def handler(websocket):
#     print(f"ESP32 ansluten! Sparar hela varv till {FILENAME}...")
    
#     with open(FILENAME, mode='a', newline='') as file:
#         writer = csv.writer(file)
        
#         try:
#             async for message in websocket:
#                 if len(message) == EXPECTED_SIZE:
#                     # Packa upp hela varvet på en gång
#                     unpacked_data = struct.unpack(STRUCT_FORMAT, message)
                    
#                     # Dela upp i distanser och valid-flaggor
#                     distances = unpacked_data[0:ENCODER_STEPS]
#                     valid_flags = unpacked_data[ENCODER_STEPS:ENCODER_STEPS*2]
                    
#                     valid_points_saved = 0
                    
#                     # Gå igenom alla 128 steg i varvet
#                     for i in range(ENCODER_STEPS):
#                         if valid_flags[i] == 1:
#                             # Räkna ut vinkeln baserat på vilket index i arrayen vi är på
#                             angle = i * (360.0 / ENCODER_STEPS)
#                             distance = distances[i]
                            
#                             # Skriv till CSV (Vinkel, Distans)
#                             writer.writerow([round(angle, 2), distance])
#                             valid_points_saved += 1
                    
#                     print(f"Sparade ett helt varv ({valid_points_saved} giltiga punkter av {ENCODER_STEPS})")
#                 else:
#                     print(f"Oväntad storlek: {len(message)} bytes (Förväntade {EXPECTED_SIZE})")
                    
#         except websockets.ConnectionClosed:
#             print("ESP32 kopplade ifrån.")

# async def main():
#     async with websockets.serve(handler, "0.0.0.0", PORT):
#         print(f"Server startad. Väntar på ESP32...")
#         await asyncio.Future()  # Kör för alltid

# if __name__ == "__main__":
#     asyncio.run(main())



import asyncio
import websockets
import struct
import cv2
import numpy as np

PORT = 8765
ENCODER_STEPS = 128
# 128 st uint16_t (2 bytes) + 128 st uint8_t (1 byte) = 384 bytes
EXPECTED_SIZE = (ENCODER_STEPS * 2) + (ENCODER_STEPS * 1)

# Fönsterinställningar
WINDOW_SIZE = 800
CENTER = WINDOW_SIZE // 2
MAX_DISTANCE_CM = 350  # Justera efter rummets storlek (3.5 meter här)
SCALE = (WINDOW_SIZE / 2) / MAX_DISTANCE_CM 

async def handler(websocket):
    print("ESP32 ansluten! Öppnar LiDAR-vy...")
    
    while True:
        try:
            message = await websocket.recv()
            
            if len(message) == EXPECTED_SIZE:
                # Packa upp: 128 stycken 'H' (uint16) och 128 stycken 'B' (uint8)
                format_str = f"<{ENCODER_STEPS}H{ENCODER_STEPS}B"
                unpacked = struct.unpack(format_str, message)
                
                distances = unpacked[0:ENCODER_STEPS]
                valid_flags = unpacked[ENCODER_STEPS:]
                
                # Skapa en tom svart bild (3 kanaler för BGR-färg)
                img = np.zeros((WINDOW_SIZE, WINDOW_SIZE, 3), dtype=np.uint8)
                
                # Rita referenscirklar för var 100:e cm
                for r_cm in [100, 200, 300]:
                    r_px = int(r_cm * SCALE)
                    cv2.circle(img, (CENTER, CENTER), r_px, (40, 40, 40), 1)
                    cv2.putText(img, f"{r_cm//100}m", (CENTER + r_px + 5, CENTER + 5), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (120, 120, 120), 1)
                
                # Rita robotens centrum (rött hårkors)
                cv2.line(img, (CENTER - 8, CENTER), (CENTER + 8, CENTER), (0, 0, 255), 1)
                cv2.line(img, (CENTER, CENTER - 8), (CENTER, CENTER + 8), (0, 0, 255), 1)
                
                valid_count = 0
                
                # Gå igenom de 128 fasta vinklarna
                for i in range(ENCODER_STEPS):
                    if valid_flags[i] == 1:
                        dist = distances[i]
                        
                        # Filtrera bort extremt brus/felmätningar
                        if dist < 10 or dist > MAX_DISTANCE_CM:
                            continue
                        
                        valid_count += 1
                        
                        # Räkna ut vinkeln i radianer (0-127 index mappat till 0-360 grader)
                        angle_deg = i * (360.0 / ENCODER_STEPS)
                        angle_rad = np.radians(angle_deg)
                        
                        # Konvertera polärt -> kartesiskt (X, Y)
                        # -cos gör att 0 grader pekar rakt "uppåt" i OpenCV-fönstret
                        x = int(CENTER + dist * SCALE * np.sin(angle_rad))
                        y = int(CENTER - dist * SCALE * np.cos(angle_rad))
                        
                        if 0 <= x < WINDOW_SIZE and 0 <= y < WINDOW_SIZE:
                            # Rita en skarp grön prick för varje träff
                            cv2.circle(img, (x, y), 3, (0, 255, 0), -1)
                
                # Visa antal punkter på skärmen
                cv2.putText(img, f"Punkter: {valid_count}/128", (15, 30), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
                
                cv2.imshow("ESP-Bot Realtime LiDAR", img)
                
                # 1ms delay krävs för att OpenCV ska hinna rita fönstret
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                print(f"Oväntad paketstorlek: {len(message)} bytes")
                
        except websockets.ConnectionClosed:
            print("ESP32 kopplade ifrån.")
            break
        except Exception as e:
            print(f"Ett fel uppstod: {e}")
            break

    cv2.destroyAllWindows()

async def main():
    async with websockets.serve(handler, "0.0.0.0", PORT):
        print(f"Server startad på port {PORT}. Väntar på ESP32...")
        await asyncio.Future()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nProgrammet avslutat av användaren.")