# Zipper
Searches best "`-mpass`" parameter for 7-Zip ZIP archives.

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
### Current limitations:
- When compressing to single archive, all files from subfolders will be in root folder of archive.
- Parameters (i. e. directories/archive name) can be only in local codepage. But found filenames can have any codepage. They will not be shown correctly in console, but should be processed fine.
- Search time is evaluated only for maximal precalculated cycle. I did not find a formula for smaller cycles (attempt was in commit [9afc972](https://github.com/VaKonS/zipper/commit/9afc972fc2c1e054012ddc0934c90ca464bf7603)), therefore only maximal possible cycle will be found.<br>It only affects time estimation, with default settings cycles smaller than maximal will end the search as intended.

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
Omit ".cycle#" in names of separate archives (detected cycle, can help find wrong results).<br>
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
**\-a** #<br>
Detect end of search by alternative methods.<br>
- *0* (default) - N identical archives in minimal found cycle. Seems to be most adequate;<br>
- *1* - *any* N identical archives. Fastest method, but *maybe* skips results. So far found results were same, but it needs more testing;<br>
- *2* - N identical archives in maximal detected cycle. I'm not sure whether minimal cycle is sufficient or not, so this option is for testing;<br>
- *3* - full minimal found cycle. Should not be needed, for testing;<br>
- *4* - full maximal detected cycle. For testing.

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
Для запуска требуется архиватор [7-Zip](http://7-zip.org/). Проверялась с версией 17.00.<br>
Для компиляции нужен пакет [TCLAP](https://sourceforge.net/projects/tclap/).<br>

---
Запуск:<br>
`zipper.exe -o "папка_для_архивов"`<br>
<br>
Сожмёт все файлы из текущего каталога в "папку для архивов".<br>
*При ошибках попробуйте заменить в путях "\\" на "/" или "\\\\".*<br>
<br>
### Ограничения на данный момент:
- При сжатии в один архив, все файлы из подкаталогов будут в корне архива.
- Параметры (имена каталогов/архива) могут задаваться только в текущей кодовой странице. Файлы, которые программа найдёт сама, могут быть с именами в любой кодовой странице. Они не будут верно отображаться, но обрабатываться должны правильно.
- Время до окончания поиска оценивается для максимального вычисленного цикла. Я не нашёл точной формулы размера меньших циклов (попытка была в коммите [9afc972](https://github.com/VaKonS/zipper/commit/9afc972fc2c1e054012ddc0934c90ca464bf7603)), поэтому вычисляется только максимальный цикл.<br>Это влияет исключительно на оценку времени, поиск при стандартных настройках будет остановлен и при нахождении меньшего цикла.

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
Ограничить количество попыток (100 по умолчанию). Установите в ноль для неограниченного поиска, пока хватит памяти (в зависимости от сжимаемых данных, поиск может быть *очень* долгим).<br>
<br>
**\-b** #<br>
Начать с # проходов. Например, если нужно продолжить поиск.<br>
<br>
**\-d** #<br>
Останавливаться при # повторах сжатия. После тестов похоже, что значения выше 12 уже ничего не меняют.<br>
<br>
**\-a** #<br>
Условие для окончания поиска:<br>
- *0* (исходное) - N совпавших архивов в минимальном найденном цикле. Вероятно, оптимальный вариант;<br>
- *1* - N *любых* совпавших архивов. Не проверяет циклы, быстрее всего, но *не исключено*, что пропускает результаты из-за меньшего числа попыток. Пока что результаты совпадают, но нужно тестирование;<br>
- *2* - N совпавших архивов в максимальном вычисленном цикле. Возможно, что минимальный цикл недостаточен и пропускает результаты, для тестового поиска до максимального цикла;<br>
- *3* - минимальный цикл с полным совпадением. Для тестирования, при значениях `-d` 12 и выше разницы с вариантом по умолчанию быть не должно;<br>
- *4* - полное совпадение в максимальном вычисленном цикле. Для тестирования.

<br>
**\-m** off/#<br>
Задать количество потоков для архиватора 7-Zip.<br>
<br>
**\-c** "7z.exe"<br>
Указать расположение архиватора 7z.exe.<br>
<br>
**\-r** "шаблон_команды"<br>
Переопределить выполняемую команду. Вместо символов `%c` шаблона подставляется имя архиватора, `%p`&nbsp;– количество проходов, `\"%i\"`&nbsp;– сжимаемый файл, `\"%o\"`&nbsp;– архив.zip. Если в команде нужен текст "%i" без подстановки, используйте "%%i".
