# Zipper
Searching best "`-mpass`" parameter for 7-Zip ZIP archives.

---
### *Warning: the process is slow and shrinks only by a few bytes.*
In other words, it's almost useless.

This program is public domain.

---
## Requirements:

[7-Zip archiver](http://7-zip.org/) by Igor Pavlov. Tested with 7-Zip v17.00.<br>
To compile, [TCLAP](https://sourceforge.net/projects/tclap/) package is needed.

---
## Basic usage:

`zipper.exe -o "output_directory"`

Will compress all files from current directory to "output_directory\file.extension.best#.of#.cycle#.zip".<br>
*Try to use "/" or "\\\\" instead of "\\" in paths in case of errors.*<br>
<br>
### Current limitation:
Do not compress subfolders to single archive.<br>
When adding files, not folders, 7-Zip will strip file paths, and **all files will be in root folder of archive.**

---
## Tuning:

**\-t** "temporary_path"<br>
Store temporary archives in specified directory. Can be set to RAM drive to avoid disk wearing.<br>
<br>
**\-l** #<br>
Limit memory usage. Default is 512 MB.<br>
<br>
**\-n** "archive.zip"<br>
Compress into single archive (instead of separate archives by default).<br>
<br>
**\-s** 0 *(zero)*<br>
Disable adding ".best#.of#" to names of separate archives.<br>
<br>
**\-f** 0 *(zero)*<br>
Omit ".cycle size/-" in names of separate archives (can help to find wrong results).<br>
<br>
**\-p** #<br>
Limit number of search passes. By default limited to 100. Set to 0 to search until memory limit (depending on compressing files, can be *very* slow).<br>
<br>
**\-b** #<br>
Begin from # passes. For example, when you need to continue a search.<br>
<br>
**\-d** #<br>
Stop after # identical archives. Values higher than 12 are probably useless.<br>
<br>
**\-a** 1<br>
Use old detection of end of search. It's faster, but *maybe* skips results. So far both methods were finding same values, but it needs more testing.<br>
<br>
**\-m** off/#<br>
Turn 7-Zip multithreading option off or limit number of threads.<br>
<br>
**\-c** "7z.exe"<br>
Manually set the pathname of 7-Zip executable.<br>
<br>
**\-r** "command_template"<br>
Redefine the archiving command. Template sequence `%c` is substituted with archiver's pathname, `%p`&nbsp;– with number of passes, `\"%i\"`&nbsp;– with compressing file, `\"%o\"`&nbsp;– with archive.zip. Use "%%i" when you need "%i" text in command without substitution.

---
## In Russian:

Подбирает `-mpass` ("количество проходов") для максимального сжатия архива ZIP.<br>
**Разница обычно в несколько байт, особого смысла в этом нет.**<br>
Но если файлов очень много, несколько килобайт ужмутся.<br>
<br>
Для запуска требуется архиватор [7-Zip](http://7-zip.org/). Проверялась с версией 17.00<br>
Для компиляции нужен пакет [TCLAP](https://sourceforge.net/projects/tclap/).<br>

---
Запуск:<br>
`zipper.exe -o "папка_для_архивов"`<br>
<br>
Сожмёт все файлы из текущего каталога в "папку для архивов".<br>
*При ошибках попробуйте заменить в путях "\\" на "/" или "\\\\".*<br>
<br>
### Ограничение на данный момент:
Не сжимайте папки в один архив.<br>
7-Zip убирает пути, если в архив добавляется файл, а не папка. **Все файлы будут в корне архива.**

---
Дополнительные параметры:<br>
<br>
**\-t** "временная папка"<br>
Делать временные архивы в указанной папке. Например, можно указать RAM диск.<br>
<br>
**\-l** #<br>
Ограничить потребление памяти (изначально 512 метров).<br>
<br>
**\-n** "архив.zip"<br>
Сжимать в один архив вместо отдельных для каждого файла.<br>
<br>
**\-s** 0 *(ноль)*<br>
Не добавлять к именам раздельных архивов ".best#.of#".<br>
<br>
**\-f** 0 *(ноль)*<br>
Не добавлять к именам раздельных архивов ".cycle#" (найденный размер цикла, помогает отследить возможно неправильные результаты).<br>
<br>
**\-p** #<br>
Ограничить количество попыток (ограничено 100 по умолчанию). Установите в ноль для неограниченного поиска, пока хватит памяти (в зависимости от сжимаемых данных, поиск может быть очень долгим).<br>
<br>
**\-b** #<br>
Начать с # проходов. Например, если нужно продолжить поиск.<br>
<br>
**\-d** #<br>
Останавливаться при # повторах сжатия. После тестов похоже, что значения выше 12 уже ничего не меняют.<br>
<br>
**\-a** 1<br>
Определять окончание поиска старым способом. Он быстрее, но не исключено, что пропускает результаты из-за меньшего числа попыток. Пока что у меня оба алгоритма дают одинаковые значения, но надо проверять.<br>
<br>
**\-m** off/#<br>
Задать количество потоков для архиватора 7-Zip.<br>
<br>
**\-c** "7z.exe"<br>
Указать расположение архиватора 7z.exe.<br>
<br>
**\-r** "шаблон_команды"<br>
Переопределить выполняемую команду. Вместо символов `%c` шаблона подставляется имя архиватора, `%p`&nbsp;– количество проходов, `\"%i\"`&nbsp;– сжимаемый файл, `\"%o\"`&nbsp;– архив.zip. Если в команде нужен текст "%i" без подстановки, используйте "%%i".
