[BITS 16]
[ORG 0x7C00]
start:
  ; Инициализация адресов сегментов. Эти операции требуется не для любого BIOS, но их рекомендуется проводить.
  mov ax, cs ; Сохранение адреса сегмента кода в ax
  mov ds, ax ; Сохранение этого адреса как начало сегмента данных
  mov ss, ax ; И сегмента стека
  mov sp, start ; Сохранение адреса стека как адрес первой инструкции этого кода. Стек будет расти вверх и не перекроет код.
  call clean

drive_bios:
  mov bx, 0x1000 ;set bufer
  mov es, bx
  mov bx, 0x0000
  mov dl, 1 ;drive num
  mov dh, 0 ;head num
  mov ch, 0 ;cylinder
  mov cl, 1 ;sector num
  mov al, 0x20 ;sector amount
  mov ah, 0x02 ;func for drive io
  int 0x13 ;interrupt to io

  ;mov bx, 0x1000 ;set bufer
  ;mov es, bx
  ;mov bx, 0x0000
  ;mov dl, 1 ;drive num
  ;mov dh, 1 ;head num
  ;mov ch, 0 ;cylinder
  ;mov cl, 1 ;sector num
  ;mov al, 18 ;sector amount
  ;mov ah, 0x02 ;func for drive io
  ;int 0x13 ;interrupt to io

  mov edi, 0xb8000 ; В регистр edi помещается адрес начала буфера видеопамяти.
  mov bx, alphabet ; В регистр bx помещается адрес начала строки
  call video_puts ; Вызывается функция для видеовывода в буфер памяти видеокарты
  jmp go_to_kernel

save_buf:
  mov bx, alphabet
  push bx

setup_protection:
  cli ; Отключение прерываний
  lgdt [gdt_info] ; Загрузка размера и адреса таблицы дескрипторов
  in al, 0x92 ; Включение адресной линии А20
  or al, 2
  out 0x92, al
  mov eax, cr0 ; Установка бита PE регистра CR0 - процессор перейдет в защищенный режим
  or al, 1
  mov cr0, eax
  jmp 0x8:protected_mode ; "Дальний" переход для загрузки корректной информации в cs (архитектурные особенности не позволяют этого сделать напрямую). 

go_to_kernel:
  mov ah, 0x00
  int 0x16
  cmp al, 0x0d ;if enter
  je save_buf

_loop:
  cmp al, [bx]
  je forbid ;if same letter
  add bx, 1
  add cx, 1
  jmp _loop

forbid:
  mov bx, cx
  mov ah, [bx]
  cmp ah, 0x20 ;space
  je put_letter
  mov ah, 0x020
  mov [bx], ah
  call clean
  call carete
  mov bx, alphabet
  mov cx, alphabet
  mov edi, 0xb8000 ; В регистр edi помещается адрес начала буфера видеопамяти.
  call video_puts
  jmp go_to_kernel

put_letter:
  mov [bx], al
  call clean
  call carete
  mov bx, alphabet
  mov cx, alphabet
  mov edi, 0xb8000 ; В регистр edi помещается адрес начала буфера видеопамяти.
  call video_puts
  jmp go_to_kernel

video_puts:
  ; Функция выводит в буфер видеопамяти (передается в edi) строку, оканчивающуюся 0 (передается в bx)
  ; После завершения edi содержит адрес по которому можно продолжать вывод следующих строк
  mov al, [bx]
  test al, al
  je video_puts_end
  mov ah, 0xDC ; Цвет символа и фона. Возможные варианты: 0x00 is black-on-black, 0x07 is lightgrey-on-black, 0x1F is white-on-blue, 0xDC is red-on-pink
  ;mov ah, 0x0e
  ;int 0x10
  mov [edi], al
  mov [edi+1], ah
  add edi, 2
  add bx, 1
  jmp video_puts

video_puts_end:
  mov bx, copy_alf ; copy str after puts
  ret

clean:
  mov ax, 0x0600 ;number of display function
  mov bh, 0xDC
  mov cx, 0x00
  mov dx, 0x184F
  ;mov al, 0x03
  int 0x10 ;clean screen
  ret

carete:
  mov ah, 2
  mov bh, 0
  mov dx, 0
  int 0x10 ;return carete
  ret

gdt:
  ; Нулевой дескриптор
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  ; Сегмент кода: base=0, size=4Gb, P=1, DPL=0, S=1(user),
  ; Type=1(code), Access=00A, G=1, B=32bit
  db 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
  ; Сегмент данных: base=0, size=4Gb, P=1, DPL=0, S=1(user),
  ; Type=0(data), Access=0W0, G=1, B=32bit
  db 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
gdt_info: ; Данные о таблице GDT (размер, положение в памяти)
  dw gdt_info - gdt ; Размер таблицы (2 байта)
  dw gdt, 0 ; 32-битный физический адрес таблицы.

alphabet:
  db "abcdefghijklmnopqrstuvwxyz", 0
copy_alf:
  db "abcdefghijklmnopqrstuvwxyz", 0

[BITS 32]
protected_mode:
  ; Загрузка селекторов сегментов для стека и данных в регистры
  mov ax, 0x10 ; Используется дескриптор с номером 2 в GDT
  mov es, ax
  mov ds, ax
  mov ss, ax
  ; Передача управления загруженному ядру
  call 0x10000 ; Адрес равен адресу загрузки в случае если ядро скомпилировано в "плоский" код

  ; Внимание! Сектор будет считаться загрузочным, если содержит в конце своих 512 байтов два следующих байта: 0x55 и 0xAA
  times (512 - ($ - start) - 2) db 0 ; Заполнение нулями до границы 512 - 2 текущей точки
  db 0x55, 0xAA ; 2 необходимых байта чтобы сектор считался загрузочным
