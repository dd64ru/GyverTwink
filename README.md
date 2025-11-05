# GyverTwink
Гирлянда на адресных светодидоах и esp8266, управление по WiFi

## Обновления
### Прошивка
- 1.1 – исправлена калибровка больше 255 светодиодов
- 1.2 – исправлена ошибка с калибровкой

### Приложение
- 1.2 – калибровка больше 255, автоматический масштаб интерфейса, поля ввода подвинул наверх, оптимизация от TheAirBlow
- 1.7 – починил связь с гирляндой ВСЁ РАБОТАЕТ

## Папки
- **docs** - всякая инфа, документы, протокол связи
- **firmware** - прошивка для esp8266
- **libraries** - библиотеки для esp8266
- **processing** - исходник приложения
- **schemes** - схемы

## Приложение (от Гайвера старое)
- **Android** - [Google Play](https://play.google.com/store/apps/details?id=ru.alexgyver.GyverTwink), [.apk](https://github.com/AlexGyver/GyverTwink/raw/main/Android/gyvertwink.apk)

## DD64 Доработки и инструкция
- Ардуино - скопировать и перезаписать все библиотеке, если что-то уставновлено локально
- Ввести логин и пароль свои
- Собрать. Гирлянда должна появиться с адресом 192.168.178.88

## DD64 Инструкция по приложению
- Нужен processing 3 версии, так же установить зависимость Andoid для версии 3
- Далее экспорт в Android Project
- В Android studio - сменить в app > build.gradle compileSdkVersion и targetSdkVersion на 33
- Добавить в defaultConfig вот это: ndk { abiFilters "arm64-v8a" } в конец объекта
- src > main > assets - удалить винду, мак, линукс
- Теперь FIle > Settings > Build > Build tools > Gradle - сменить Gradle user home на C:/Processing/Gradle/gradle-6.5
- В той же вкладке ищем Distribution > Local > Пишем C:/Processing/Gradle/gradle-6.5. В эту папку скачиваем и кладем gradle-6.5-all.zip и разархивируем
- Все в той же вкладке теперь Gradle JDK выбираем или скачиваем - Версия 11, вендор Eclipse Temurin
- Apply. Ok. Закрываем Студию, откываем, ждем Gradle, теперь ругаться не должен
- File > Build > Generate Signed App Bundle or APK > APK > Создаем любой ключ с рандомным паролем, заполняем на что ругается > Далее Release и Create
- Лента всегда будет по адресу 192.168.178.88 - лучше в роутере выделить ей постоянный этот адрес
