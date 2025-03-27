#!/bin/bash
set -e # Прерывать выполнение при ошибке

# Название проекта (для удобства)
PROJECT_NAME="snmp_converter"
# Путь к исполняемому файлу (после компиляции)
OUTPUT_BINARY="RS485_2"

# Список зависимостей для установки
DEPENDENCIES=(
    "build-essential"
    "qt5-default"
    "qtserialport5-dev"
    "libqt5network5"
    "qtbase5-dev"
    "qtchooser"
)

# Проверка и установка зависимостей
echo "Проверка зависимостей..."
for pkg in "${DEPENDENCIES[@]}"; do
    if ! dpkg -s "$pkg" &> /dev/null; then
        echo "Устанавливаем $pkg..."
        sudo apt-get install -y "$pkg"
    else
        echo "$pkg уже установлен"
    fi
done

# Компиляция проекта
echo "Компиляция проекта..."
qmake "$PROJECT_NAME.pro"
make

# Проверка результата компиляции
if [ -f "$OUTPUT_BINARY" ]; then
    echo -e "\n\033[32mКомпиляция успешно завершена!\033[0m"
    echo "Исполняемый файл: $PWD/$OUTPUT_BINARY"
else
    echo -e "\n\033[31mОшибка компиляции!\033[0m" >&2
    exit 1
fi
