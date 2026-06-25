# CYD Bike Computer — Usage Guide

[Русский](#русский) · [English](#english)

---

## Русский

Полное руководство по использованию велокомпьютера на плате CYD
(`ESP32-2432S028R`).

### 1. Что в комплекте (железо)

- Плата **CYD ESP32-2432S028R** — экран 2.8", резистивный тач, слот microSD.
- **MPU6050** — акселерометр/гироскоп (скорость, каденс, уклон, наклон).
- **BMP180** — барометр (температура, давление, высота, набор высоты).
- **Сервопривод** на рычаг переключения (опционально).
- Одна батарея на плату и сервопривод вместе.

Подключение: датчики на шине I2C (SDA = GPIO21, SCL = GPIO22), сервопривод на
GPIO1, microSD по SPI (CS = GPIO5).

### 2. Первый запуск

1. Прошей плату **по USB** (это обязательно для первого раза):
   ```bash
   pio run --target upload
   ```
2. При включении пройдёт калибровка датчика движения — **не трогай велосипед
   несколько секунд**, пока на экране идёт заставка/калибровка.
3. Дальше появится главный экран.

### 3. Экраны и навигация

Экраны листаются **свайпом влево/вправо** по тачскрину или кнопкой **PAGE** в
верхней панели:

1. **SPEED** — крупная скорость, часы, TRIP / MAX / AVG, текущая передача,
   мини-график скорости.
2. **TELEMETRY** — таблица: одометр, время в пути, уклон %, каденс, температура
   (мин/макс), давление, высота.
3. **GRAPH** — график истории скорости.
4. **ALTITUDE** — график высоты, набор высоты, тренд давления.

Верхняя панель: **PAGE** (следующий экран), **MENU** (настройки), **INV**
(инверсия цветов).

### 4. Переключение передач

- **Вручную:** джойстик вправо — выше передача, влево — ниже (удерживание
  повторяет).
- **Автоматически:** включи **Auto-shift** в меню — передачи меняются по
  скорости.
- **Калибровка:** меню → **Servo calib** — выбираешь передачу и крутишь угол
  сервопривода кнопками −10 / −1 / +1 / +10, сохраняешь. Значения хранятся в
  NVS.

### 5. Меню настроек

Меню (кнопка **MENU**) состоит из 4 страниц, листается **PREV / NEXT**:

- **Параметры:** единицы (км/ч ⇄ mph), яркость, чувствительность тача, порог
  предупреждения о скорости, число передач, авто-переключение, калибровка
  скорости (%), **масса райдера**, **интервал смазки цепи**, **интервал
  сервиса**, инверсия цветов.
- **Действия:** сброс TRIP, сброс одометра, **«цепь смазана»**, **«сервис
  сделан»**, **Records**, **Ride summary**, **Ride history**, **Servo calib**,
  выход.

### 6. Рекорды, итоги, история

- **Records** — рекорды за всё время (макс. скорость, самый длинный заезд,
  макс. набор высоты) + статус ТО. Хранятся в NVS, сбрасываются на этом экране
  или из веба.
- **Ride summary** — итоги текущего заезда: дистанция, время, средняя/макс
  скорость, набор высоты, средний каденс, мощность, калории.
- **Ride history** — список CSV-файлов с microSD; тап по файлу открывает заезд
  и **строит графики скорости и высоты прямо на экране CYD**.

### 7. Мощность и калории

Оценка мощности считается из массы райдера, уклона, скорости и ускорения и
интегрируется в килокалории. Чтобы цифры были ближе к правде, задай свою
**массу** (меню или веб) и при желании подправь коэффициенты `POWER_*` в
`config.h`.

### 8. Обслуживание (ТО)

- Интервалы смазки цепи и сервиса задаются в меню или в вебе.
- Когда пробег подходит к интервалу, на главном экране появляется метка
  **OIL**, а на экране Records — **OVERDUE** (красным).
- После смазки/сервиса нажми **«цепь смазана»** / **«сервис сделан»** —
  счётчик сбросится, событие запишется в журнал `MAINT.LOG` на SD и в NVS.

### 9. Wi-Fi дашборд

1. Подключись к точке доступа **`CYD-Bike`** (пароль `ride1234`).
2. Открой в браузере **`http://192.168.4.1`**.

На странице: живая телеметрия, рекорды (+сброс), обслуживание (+кнопки),
настройки, **тюнинг порогов скорости/вибрации без перепрошивки**, список
сохранённых заездов с **скачиванием CSV в телефон** и построением графика в
браузере, кнопка **Sync time** (поставить часы из браузера) и ссылка на
обновление прошивки.

SSID/пароль меняются в `config.h` (`WIFI_AP_SSID` / `WIFI_AP_PASS`).

### 10. Домашняя сеть (опционально) и часы

- Заполни `WIFI_STA_SSID` / `WIFI_STA_PASS` в `config.h` — плата подключится к
  твоей сети, не выключая свою точку доступа.
- После подключения время берётся по **NTP** (`NTP_SERVER`), и часы работают
  без захода на дашборд. Если STA-поля пустые — остаётся только точка доступа,
  а время можно поставить кнопкой **Sync time**.

### 11. Обновление прошивки по Wi-Fi (OTA)

1. Собери прошивку: `pio run` → файл `.pio/build/<env>/firmware.bin`.
2. Открой дашборд → **Firmware update → open updater** (или
   `http://192.168.4.1/update`).
3. Выбери `firmware.bin`, нажми **Upload**, дождись прогресс-бара.
4. Плата сама перепрошьётся и перезагрузится.

Первая прошивка всегда по USB. Не отключай питание во время заливки.
Выключается флагом `FEATURE_OTA 0`.

### 12. Логи на SD

Если вставлена microSD и `FEATURE_SD = 1`, при каждом включении создаётся новый
пронумерованный CSV (`RIDE8324.CSV` → `RIDE8325.CSV`) с записью всех данных раз
в секунду. Столбцы: epoch, время, скорость, передача, температура, давление,
высота, уклон, каденс, дистанция, тангаж, крен.

### 13. Яркость и сон

- Яркость регулируется **затемнением цвета пикселей**, а не PWM (на CYD
  подсветка делит линию с I2C).
- После простоя экран гаснет до тусклых часов и просыпается от движения или
  касания.

### 14. Если что-то не так

- **Скорость врёт** — проверь массу и калибровку скорости (%), подстрой пороги
  вибрации в разделе Tuning.
- **Серво дёргается/проседает питание** — это ожидаемо при общей батарее,
  переключение идёт ступеньками с паузами; убедись, что источник тянет пусковой
  ток.
- **Нет отладки в Serial** — это нормально: GPIO1 занят сервоприводом
  (UART0 TX).
- **OTA не грузится на новую плату** — сначала нужна одна прошивка по USB.

---

## English

Full usage guide for the CYD (`ESP32-2432S028R`) bike computer.

### 1. Hardware

- **CYD ESP32-2432S028R** board — 2.8" screen, resistive touch, microSD slot.
- **MPU6050** — accelerometer/gyro (speed, cadence, grade, tilt).
- **BMP180** — barometer (temperature, pressure, altitude, elevation gain).
- **Servo** on the shifter lever (optional).
- One battery shared by the board and the servo.

Wiring: sensors on I2C (SDA = GPIO21, SCL = GPIO22), servo on GPIO1, microSD over
SPI (CS = GPIO5).

### 2. First run

1. Flash over **USB** (required the first time):
   ```bash
   pio run --target upload
   ```
2. On power-up the motion sensor calibrates — **keep the bike still for a few
   seconds** during the splash/calibration screen.
3. The main screen then appears.

### 3. Screens & navigation

Swipe **left/right** on the touchscreen or use the **PAGE** button in the top
bar:

1. **SPEED** — big speed, clock, TRIP / MAX / AVG, current gear, mini speed
   graph.
2. **TELEMETRY** — odometer, ride time, grade %, cadence, temperature
   (min/max), pressure, altitude.
3. **GRAPH** — speed history.
4. **ALTITUDE** — altitude graph, elevation gain, pressure trend.

Top bar: **PAGE** (next screen), **MENU** (settings), **INV** (invert colours).

### 4. Gear shifting

- **Manual:** joystick right = up, left = down (hold to repeat).
- **Automatic:** enable **Auto-shift** in the menu to shift by speed.
- **Calibration:** menu → **Servo calib** — pick a gear and adjust the servo
  angle with −10 / −1 / +1 / +10, then save. Values are stored in NVS.

### 5. Settings menu

The **MENU** has 4 pages, navigated with **PREV / NEXT**:

- **Values:** units (km/h ⇄ mph), brightness, touch sensitivity, speed warning
  threshold, number of gears, auto-shift, speed calibration (%), **rider
  mass**, **chain-lube interval**, **service interval**, invert colours.
- **Actions:** reset TRIP, reset odometer, **chain lubed**, **service done**,
  **Records**, **Ride summary**, **Ride history**, **Servo calib**, exit.

### 6. Records, summary, history

- **Records** — all-time bests (top speed, longest ride, biggest climb) plus
  maintenance status. Stored in NVS, resettable here or from the web.
- **Ride summary** — current ride: distance, time, average/max speed,
  elevation gain, average cadence, power, calories.
- **Ride history** — lists CSV files on the microSD; tap one to open the ride
  and **plot its speed and altitude right on the CYD**.

### 7. Power & calories

A power estimate is derived from rider mass, grade, speed and acceleration, then
integrated into kilocalories. Set your **mass** (menu or web) and optionally
tune the `POWER_*` constants in `config.h` for better accuracy.

### 8. Maintenance

- Set chain-lube and service intervals in the menu or web.
- As the distance approaches an interval, an **OIL** marker appears on the main
  screen and **OVERDUE** (red) on the Records screen.
- After servicing, tap **chain lubed** / **service done** — the counter resets
  and the event is logged to `MAINT.LOG` on SD and to NVS.

### 9. Wi-Fi dashboard

1. Join the **`CYD-Bike`** access point (password `ride1234`).
2. Open **`http://192.168.4.1`** in a browser.

The page shows live telemetry, records (+reset), maintenance (+buttons),
settings, **live tuning of speed/vibration thresholds without reflashing**, the
list of saved rides with **CSV download to your phone** and in-browser charts, a
**Sync time** button, and a link to the firmware updater.

Change the SSID/password in `config.h` (`WIFI_AP_SSID` / `WIFI_AP_PASS`).

### 10. Home network (optional) & clock

- Fill in `WIFI_STA_SSID` / `WIFI_STA_PASS` in `config.h` and the board joins
  your network while keeping its own access point.
- Once connected the time is fetched over **NTP** (`NTP_SERVER`), so the clock
  works without opening the dashboard. With STA empty it stays access-point
  only, and you can set the time with the **Sync time** button.

### 11. Over-the-air firmware update (OTA)

1. Build: `pio run` → `.pio/build/<env>/firmware.bin`.
2. Open the dashboard → **Firmware update → open updater** (or
   `http://192.168.4.1/update`).
3. Pick `firmware.bin`, press **Upload**, wait for the progress bar.
4. The board flashes and reboots itself.

The first flash is always over USB. Don't cut power mid-upload. Disable with
`FEATURE_OTA 0`.

### 12. SD logging

With a microSD inserted and `FEATURE_SD = 1`, each power-up creates a new
numbered CSV (`RIDE8324.CSV` → `RIDE8325.CSV`) and logs every value once a
second. Columns: epoch, time, speed, gear, temperature, pressure, altitude,
grade, cadence, distance, pitch, roll.

### 13. Brightness & sleep

- Brightness is done by **dimming pixel colours**, not PWM (the backlight on
  the CYD shares a line with I2C).
- After idling, the screen blanks to a dim clock and wakes on motion or touch.

### 14. Troubleshooting

- **Speed looks off** — check rider mass and speed calibration (%), and adjust
  the vibration thresholds in the Tuning section.
- **Servo stutters / power dips** — expected on a shared battery; shifting uses
  stepped moves with rest pauses, and the supply must handle stall current.
- **No Serial debug** — by design: GPIO1 is used by the servo (UART0 TX).
- **OTA won't load on a fresh board** — do one USB flash first.
