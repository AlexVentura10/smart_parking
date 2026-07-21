import cv2
import numpy as np

def convert_image_to_header(img_path, output_header_path, var_name):
    # Carrega a imagem e redimensiona para 64x64
    img = cv2.imread(img_path)
    img = cv2.resize(img, (64, 64))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    
    # Converte para int8 [-128 a 127] conforme exigido pelo modelo quantizado
    img_int8 = img.astype(np.int32) - 128
    flat_data = img_int8.flatten().astype(np.int8)
    
    with open(output_header_path, 'w') as f:
        f.write(f'#ifndef {var_name.upper()}_H\n#define {var_name.upper()}_H\n\n')
        f.write(f'// Imagem: {img_path}\n')
        f.write(f'const int8_t {var_name}[] = {{\n  ')
        for i, val in enumerate(flat_data):
            f.write(f'{val}, ')
            if (i + 1) % 16 == 0:
                f.write('\n  ')
        f.write('\n};\n')
        f.write(f'const unsigned int {var_name}_len = {len(flat_data)};\n\n')
        f.write('#endif\n')

# Gerar amostras de teste
convert_image_to_header('test_samples/empty_spot_01.jpg', 'main/sample_empty.h', 'sample_empty')
convert_image_to_header('test_samples/occupied_spot_01.jpg', 'main/sample_occupied.h', 'sample_occupied')