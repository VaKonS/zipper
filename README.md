# Zipper
Searching best "`-mpass`" parameter for 7-Zip ZIP archives.

---
### *Warning: the process is slow and shrinks only by a few bytes.*
In other words, it's almost useless.

This program is public domain.

---
## Requirements:

[7-Zip archiver](http://7-zip.org/) by Igor Pavlov.<br>
To compile, [TCLAP](https://sourceforge.net/projects/tclap/) package is needed.

---
## Basic usage:

`zipper.exe -o "output_directory"`

Will compress all files from current directory to "output_directory\file.extension.best#.of#.zip".<br>
*Try to use "/" or "\\\\" instead of "\\" in paths in case of errors.*

---
## Tuning:

**\-t** "temporary_path"<br>
Store temporary archives in specified directory. Can be set to RAM drive to avoid disk wearing.<br>
<br>
**\-l** #<br>
Limit memory usage. Default is 512 MB.<br>
<br>
**\-s** 0 *(zero)*<br>
Disable adding ".best#.of#" to archive names.<br>
<br>
**\-p** #<br>
Limit number of search passes. Large values can be very slow.<br>
<br>
**\-b** #<br>
Begin from # passes. For example, when you need to continue a search.<br>
<br>
**\-m** off/#<br>
Turn 7-Zip multithreading option off or limit number of threads.<br>
<br>
**\-d** #<br>
Threshold to detect compression cycling. Values higher than 24 are probably useless.<br>
<br>
**\-c** "7-Zip_pathname"<br>
Manually set the pathname of 7-Zip executable.<br>
<br>
**\-r** "command_template"<br>
Redefine the archiving command. Template sequence `%c` is substituted with archiver's pathname, `%p`&nbsp;– with number of passes, `\"%i\"`&nbsp;– with compressing file, `\"%o\"`&nbsp;– with archive.zip.

---
## In Russian:

Подбирает `-mpass` ("количество проходов") для максимального сжатия архива ZIP.<br>
**Разница обычно в несколько байт, особого смысла в этом нет.**<br>
Но если файлов очень много, несколько килобайт ужмутся.<br>
<br>
Для запуска требуется архиватор [7-Zip](http://7-zip.org/).<br>
Для компиляции нужен пакет [TCLAP](https://sourceforge.net/projects/tclap/).<br>

---
Запуск:<br>
`zipper.exe -o "папка_для_архивов"`<br>
<br>
Сожмёт все файлы из текущего каталога в "папку для архивов".<br>
*При ошибках попробуйте заменить в путях "\\" на "/" или "\\\\".*

---
Дополнительные параметры:<br>
<br>
**\-t** "временная папка"<br>
Делать временные архивы в указанной папке. Например, можно указать RAM диск.<br>
<br>
**\-l** #<br>
Ограничить потребление памяти (изначально 512 метров).<br>
<br>
**\-s** 0 *(ноль)*<br>
Не добавлять к именам архивов ".best#.of#".<br>
<br>
**\-p** #<br>
Ограничить количество попыток (100 по умолчанию).<br>
<br>
**\-b** #<br>
Начать с # проходов. Например, если нужно продолжить поиск.<br>
<br>
**\-m** off/#<br>
Задать количество потоков для архиватора 7-Zip.<br>
<br>
**\-d** #<br>
Останавливаться при повторах сжатия. После тестов похоже, что значения выше 12 уже ничего не меняют.<br>
<br>
**\-c** "7z.exe"<br>
Указать расположение архиватора 7z.exe.<br>
<br>
**\-r** "шаблон_команды"<br>
Переопределить выполняемую команду. Вместо символов `%c` шаблона подставляется имя архиватора, `%p`&nbsp;– количество проходов, `\"%i\"`&nbsp;– сжимаемый файл, `\"%o\"`&nbsp;– архив.zip.
