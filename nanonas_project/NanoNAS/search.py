from NanoNAS import NanoNAS

input_shape = (50, 50, 3)  # Confirme se as imagens do PKLot Patches são 50x50

# Caminhos atualizados para o seu novo dataset PKLot
path_to_training_set = '/home/alex_ventura/Mestrado/projetos_ia/pklot_dataset/archive/Patches/train'
path_to_test_set = '/home/alex_ventura/Mestrado/projetos_ia/pklot_dataset/archive/Patches/test'
val_split = 0.2  # 20% para validação durante a busca

# Se o dataset for muito grande para caber inteiro na RAM, mude para False
cache = True

# Target: ESP32-S3 (Ajuste conservador seguro para TinyML: ex: 320 KB RAM, 1.5 MB Flash)
ram_upper_bound = 327680     # 320 kiB (O ESP32-S3 possui alta RAM disponível)
flash_upper_bound = 1572864  # 1.5 MiB para o modelo quantizado
MACC_upper_bound = 15000000  # Limite ajustado para o poder de processamento do ESP32-S3

# Inicialização do NanoNAS
nanoNAS = NanoNAS(
    max_ram=ram_upper_bound,
    max_flash=flash_upper_bound,
    max_macc=MACC_upper_bound,
    path_to_training_set=path_to_training_set,
    val_split=val_split,
    cache=cache,
    input_shape=input_shape,
    save_path='./results_pklot'
)

# 1. Busca pela arquitetura ideal
nanoNAS.search(save_search_history=False)

# 2. Treinamento da melhor arquitetura encontrada
nanoNAS.train(training_epochs=50, training_learning_rate=0.001, training_batch_size=32)

# 3. Aplicação da quantização pós-treinamento para INT8 (TFLite)
nanoNAS.apply_uint8_post_training_quantization()

# 4. Avaliação dos modelos Keras e TFLite gerados
nanoNAS.test_keras_model(path_to_test_set)
nanoNAS.test_tflite_model(path_to_test_set)