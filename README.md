# Практична робота Ethernet

**Пристрій:** STM32F411 Black Pill  
**Транспорт:** Ethernet (W5500 SPI)  
**Протокол:** HTTP/JSON  
**Оновлення:** 1 Hz без миготіння

---

## Підключення обладнання (Pin Mapping)

### W5500 Ethernet Module (SPI2)
| W5500 Pin | STM32 Pin | Опис |
|-----------|-----------|------|
| MOSI | PB15 | SPI2 MOSI |
| MISO | PB14 | SPI2 MISO |
| SCK | PB13 | SPI2 SCK |
| CS | PB12 | Chip Select |
| RST | PB11 | Reset |
| INT | PB10 | Interrupt (опційно) |
| 3.3V | 3.3V | Живлення |
| GND | GND | Земля |

### ILI9341 Display (SPI1)
| ILI9341 Pin | STM32 Pin |
|-------------|-----------|
| MOSI | PA7 |
| SCK | PA5 |
| CS | PA4 |
| DC | PA3 |
| RST | PA2 |

### I²C Sensor (I2C1)
| Сенсор | Адреса | Pins |
|--------|--------|------|
| BME280 | 0x76 | PB6 (SCL), PB7 (SDA) |

### GPS Module
| GPS Pin | STM32 Pin |
|---------|-----------|
| TX | PA10 (USART1_RX) |
| RX | PA9 (USART1_TX) |

### CLI Debug (USART2)
| Pin | STM32 Pin |
|-----|-----------|
| TX | PA2 (USART2_TX) |
| RX | PA3 (USART2_RX) |

---

## Мережеві параметри

### Режими роботи

**1. DHCP (За замовчуванням)**
- Автоматичне отримання IP від DHCP-сервера
- Timeout: 30 секунд
- При невдачі → перехід на статичний режим

**2. Static IP (Fallback)**
- IP: `192.168.1.177`
- Subnet: `255.255.255.0`
- Gateway: `192.168.1.1`
- DNS: `8.8.8.8`

### Перевірка підключення

```bash
# 1. Ping test
ping 192.168.1.177

# 2. HTTP test
curl http://192.168.1.177/status

# 3. Відкрить в браузері
http://192.168.1.177/
```

---

| Задача | Період | Пріоритет |
|--------|--------|-----------|
| Sensor poll | 1000 ms | Medium |
| Display update | 1000 ms | Low |
| HTTP process | Loop | High |
| DHCP run | 100 ms | Medium |
| CLI process | Loop | Low |

---


### Відео з демонстрацією роботи основного завдання:

https://drive.google.com/file/d/1Kl5WMtwf7H1Yn9W5Zftcp3ezisavrfD7/view?usp=sharing

--- 

## Додаткові завдання:

