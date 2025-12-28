import asyncio
import websockets
import struct
import csv

# Inställningar
PORT = 8080
LIDAR_PACKET_LENGTH = 100
EXPECTED_SIZE = LIDAR_PACKET_LENGTH * 6
FILENAME = "lidar_data.csv"

async def handler(websocket):
    print(f"ESP32 ansluten! Sparar data till {FILENAME}...")
    
    # Öppna filen i "append"-läge
    with open(FILENAME, mode='a', newline='') as file:
        writer = csv.writer(file)
        
        try:
            async for message in websocket:
                if len(message) == EXPECTED_SIZE:
                    format_str = "<" + "fH" * LIDAR_PACKET_LENGTH
                    data = struct.unpack(format_str, message)
                    
                    # Para ihop vinklar och distanser
                    angles = data[0::2]
                    distances = data[1::2]
                    
                    # Spara varje mätpunkt som en rad i CSV-filen
                    for a, d in zip(angles, distances):
                        writer.writerow([round(a, 2), d])
                    
                    print(f"Sparade paket ({LIDAR_PACKET_LENGTH} punkter)")
                else:
                    print(f"Oväntad storlek: {len(message)}")
        except websockets.ConnectionClosed:
            print("ESP32 kopplade ifrån.")

async def main():
    # Starta servern (IP 0.0.0.0 lyssnar på allt)
    async with websockets.serve(handler, "0.0.0.0", PORT):
        print(f"Server startad. Väntar på ESP32...")
        await asyncio.Future()

if __name__ == "__main__":
    # Rensa filen vid start så vi inte blandar gamla mätningar
    with open(FILENAME, mode='w', newline='') as file:
        pass 
    asyncio.run(main())