import struct
import re
import os

OUTPUT_FOLDER = "bin_files"

def txt_to_bin(input_file, output_file):
    # Leer el archivo txt
    with open(input_file, 'r') as f:
        content = f.read()
    
    # Extraer todos los valores hexadecimales
    values = re.findall(r'0[xX][0-9a-fA-F]+', content)
    
    if not values:
        print(f"No se encontraron valores hex en {input_file}")
        return
    
    # Convertir y escribir como binario (uint16_t, little-endian)
    with open(output_file, 'wb') as f:
        for v in values:
            f.write(struct.pack('<H', int(v, 16)))  # '<H' = uint16_t little-endian
    
    print(f"✓ {input_file} → {output_file}  ({len(values)} valores)")

def verify_bin(txt_file, bin_file, n=5):
    with open(txt_file, 'r') as f:
        values_txt = re.findall(r'0[xX][0-9a-fA-F]+', f.read())
    
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    print("Primeros valores:")
    for i in range(min(n, len(values_txt))):
        original = int(values_txt[i], 16)
        leido    = struct.unpack('<H', data[i*2 : i*2+2])[0]
        match    = "✓" if original == leido else "✗"
        print(f"  [{i}] txt: 0x{original:04X}  bin: 0x{leido:04X}  {match}")

# --- Convertir un solo archivo ---
# txt_to_bin("load_screen.txt", "load_screen.bin")
# verify_bin("load_screen.txt", "load_screen.bin")

# Crear carpeta si no existe
os.makedirs(OUTPUT_FOLDER, exist_ok=True)

# --- O convertir TODOS los .txt de la carpeta actual ---
for file in os.listdir('.'):
    if file.endswith('.txt'):
        output = os.path.join(OUTPUT_FOLDER, file.replace('.txt', '.bin'))
        txt_to_bin(file, output)
        verify_bin(file, output)