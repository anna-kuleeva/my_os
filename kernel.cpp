// Эта инструкция обязательно должна быть первой, т.к. этот код компилируется в бинарный,
// и загрузчик передает управление по адресу первой инструкции бинарного образа ядра ОС.
__asm("jmp kmain");
#define VIDEO_BUF_PTR (0xb8000)
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
#define GDT_CS (0x8) // Селектор секции кода, установленный загрузчиком ОС
#define PIC1_PORT (0x20)
// Базовый порт управления курсором текстового экрана. Подходит для большинства, но может отличаться в других BIOS и в общем случае адрес должен быть прочитан из BIOS data area.
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина текстового экрана

struct idt_entry { // Структура описывает данные об обработчике прерывания
  unsigned short base_lo; // Младшие биты адреса обработчика
  unsigned short segm_sel; // Селектор сегмента кода
  unsigned char always0; // Этот байт всегда 0
  unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы -- это константы - IDT_TYPE...
  unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено

struct idt_ptr { // Структура, адрес которой передается как аргумент команды lidt
  unsigned short limit;
  unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp; // Описатель таблицы для команды lidt

typedef void (*intr_handler)();
void default_intr_handler();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr);
void intr_init();
void intr_start();
void intr_enable();
void intr_disable();
void keyb_init();
void keyb_handler();
void keyb_process_keys();
void cursor_moveto(unsigned int strnum, unsigned int pos);
void out_str(int color, const char* ptr, unsigned int strnum);
void on_key(unsigned char scan_code);

unsigned int strncmp (unsigned char *a, unsigned char *b, unsigned int n);
unsigned int strlen (const char *string);
unsigned char* itoa(unsigned int num);
//unsigned char* strcat(char *dst, char *src);
void choose_command(unsigned char* command);
//void get_alfa(char* pointer);
void clean(unsigned int begin, unsigned int end) ;
void info();
void dictinfo();
void wordstat(unsigned char c);
void translate(unsigned char *word);
void shutdown();
void puts(int color, unsigned char ptr, unsigned int strnum, unsigned int pos);
unsigned char helper(unsigned int x);

void default_intr_handler() { // Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону
  asm("pusha");
  // ... (реализация обработки)
  asm("popa; leave; iret");
}

unsigned char alphabet[26] = { 0 };
//unsigned char new_alphabet[26] = { 0 };
unsigned int pos = 0;
unsigned int loaded = 0;

unsigned char syms[] = { 0 /*no a[0] in ascii*/, 0, /*esc*/
'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, //backspace
0 /*tab*/, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, //enter
0 /*ctrl*/, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, //shift
'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, //shift
0 /*prtsc*/, 0 /*alt*/, ' ' /*space*/, 0 /*caps*/};


const char *english[52] = {
"angry", //0
"art",  //1
"black", //2
"blue", //3
//"bye",
"cat", //4
//"car",
"cinema", //5
//"coffee",
//"cup",
"dad", //6
"day", //7
//"dog",
"east", //8
"experience", //9
"false", //10
"family", //11
//"fish",
//"flower",
//"food",
"god", //12
"green", //13
//"happy",
"hate", //14
//"he",
"hello", //15
"i",
"information",
"jungle",
"justice",
"kind",
"knowledge",
"literature",
"love",
"mom",
"moon",
"music",
"never",
"night",
"operation",
"os",
"plane",
"plate",
"question",
"queue",
"rat",
"red",
"sad",
"security",
//"she",
//"spoon",
//"study",
//"sun",
//"tea",
"theater",
//"train",
"true",
"university",
"useless",
"virus",
"vote",
"water",
//"we",
"white",
"xenophobia",
"xylophone",
"yellow",
"you"
"zombie",
"zoo"};

const char *spanish[52] = {
"enojado", //0
"arte", //1
"negro", //2
"azul", //3
//"adios",
"gato", //4
//"coche",
"cine", //5
//"cafe",
//"taza",
"Papa", //6
"dia", //7
//"perro",
"Oriente", //8
"experiencia", //9
"falso", //10
"familia", //11
//"pescado",
//"flor",
//"alimento",
"Dios", //12
"verde", //13
//"feliz",
"odiar", //14
//"he",
"hola", //15
"me",
"informacion",
"selva",
"justicia",
"tipo",
"conocimiento",
"literatura",
"amor",
"madre",
"Luna",
"sica",
"nunca",
"noche",
"operacion",
"operativo",
"avion",
"placa",
"pregunta",
"cola",
"rata",
"rojo",
"triste",
"seguridad",
//"ella",
//"cuchara",
//"estudio",
//"sol",
//"te",
"teatro",
//"tren",
"verdadero",
"Universidad",
"inutil",
"virus",
"voto",
"agua",
//"nos",
"blanco",
"xenofobia",
"xilofono",
"amarillo",
"usted"
"zombie",
"zoologico"};

void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr) { //regestration of interrupts (drive, keyboard, timer...)
  unsigned int hndlr_addr = (unsigned int) hndlr;
  g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
  g_idt[num].segm_sel = segm_sel;
  g_idt[num].always0 = 0;
  g_idt[num].flags = flags;
  g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

void intr_init() { // Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
  int i;
  int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
  for(i = 0; i < idt_count; i++)
    intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR,
default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start() { //regestration of descriptors table
  int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
  g_idtp.base = (unsigned int) (&g_idt[0]);
  g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
  asm("lidt %0" : : "m" (g_idtp) );
}

void intr_enable() {
  asm("sti");
}

void intr_disable() {
  asm("cli");
}

static inline unsigned char inb (unsigned short port) { // Чтение из порта
  unsigned char data;
  asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
  return data;
}

static inline void outb (unsigned short port, unsigned char data) { // Запись
  asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline void outw (unsigned short port, unsigned int data) { // Запись
  asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void keyb_init() {
  // Регистрация обработчика прерывания
  intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
  // Разрешение только прерываний клавиатуры от контроллера 8259
  outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура).
  // Разрешены будут только прерывания, чьи биты установлены в 0
}

void keyb_handler() {
  asm("pusha");
  // Обработка поступивших данных
  keyb_process_keys();
  // Отправка контроллеру 8259 нотификации о том, что прерывание обработано
  outb(PIC1_PORT, 0x20);
  asm("popa; leave; iret");
}

void keyb_process_keys() {
  // Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
  if (inb(0x64) & 0x01) {
    unsigned char scan_code;
    unsigned char state;
    scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
    if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
      on_key(scan_code);
  }
}

void on_key(unsigned char scan_code) {
  switch (scan_code) {
    case 14: {
      unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
      video_buf += 80*2 * 9 + 2*(pos - 1); //move on 2 bytes (color + char)
      video_buf[0] = 0; // Символ (код)
      video_buf[1] = 0xDC; // Цвет символа и фона
      //video_buf -= 2;
      pos--;
      break;
    }
    case 28: { //enter
      unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
      video_buf += 80*2 * 9;
      unsigned char* string;
      unsigned char j;
      for (unsigned int i = 0, j = 0; i < 40; i+=2, j++)
        string[j] = video_buf[i]; // Символ (код)
      //string[j] = '1';
      choose_command(string);
      break;
     }
    case 56: { //alt
      clean(9, 15);
      pos = 0;
      break;
     }
    default:
      puts(0xDC, scan_code, 9, pos);
      pos++;
      break;
  }
}

// Функция переводит курсор на строку strnum (0 – самая верхняя) в позицию pos на этой строке (0 – самое левое положение).
void cursor_moveto(unsigned int strnum, unsigned int pos) {
  unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
  outb(CURSOR_PORT, 0x0F);
  outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
  outb(CURSOR_PORT, 0x0E);
  outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
}

void out_str(int color, const char* ptr, unsigned int strnum) {
  unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
  video_buf += 80*2 * strnum;
  while (*ptr) {
    video_buf[0] = (unsigned char) *ptr; // Символ (код)
    video_buf[1] = color; // Цвет символа и фона
    video_buf += 2;
    ptr++;
  }
}

void puts(int color, unsigned char ptr, unsigned int strnum, unsigned int pos) {
  if (pos < 40) {
    unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
    video_buf += 80*2 * strnum + 2*pos; //move on 2 bytes (color + char)
    video_buf[0] = syms[ptr]; // Символ (код)
    video_buf[1] = color; // Цвет символа и фона
    video_buf += 2;
  }
}

unsigned int strncmp (unsigned char *a, unsigned char *b, unsigned int n) {
  for (unsigned int i = 0; i < n; i++) {
    if (a[i] == '\0' || b[i] == '\0') break; //return 0; //str is shorter than need
    if (a[i] != b[i]) return 0; //there is some difference
  }
  return 1; //strings are equal
}

unsigned int strlen (const char *string) {
  const char *p;
  if (string == '\0') {
    const char* error = "ERROR! String = NULL!";
    out_str(0xDC, error, 10);
    return -1;
  }

  unsigned int cnt = 0;
  for (p = string; /**p != '\0'*/ (*p >= 'a') && (*p <= 'z'); p++, cnt++)
    continue;
  return cnt; //p - string;
}

void choose_command(unsigned char* command) {
  if (strncmp(command, (unsigned char*) "info", 5))
    info();
  else if (strncmp(command, (unsigned char*) "dictinfo", 9))
    dictinfo();
  else if (strncmp(command, (unsigned char*) "shutdown", 9))
    shutdown();
  else if (strncmp(command, (unsigned char*) "translate", 10)) {
    unsigned char arg[20] = { 0 };
    for (unsigned int i = 0; i < 20; i++) {
      arg[i] = command[i + 10];
    }
    translate(arg);
  }
  else if (strncmp(command, (unsigned char*) "wordstat", 9)) {
    unsigned char c = command[9];
    wordstat(c);
  }
  else {
    clean(10, 15);
    const char* empty = "Unknown command";
    out_str(0xDC, empty, 10);
  }
}

/*void get_alfa(unsigned char* pointer) { //get arguments from loader
  unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
  unsigned char* buf;
  //buf = pointer;
  for (unsigned int i = 0; i < 26; i++) { 
    pointer[i] = (unsigned char)video_buf[0];
    video_buf += 2;
  }
  pointer[26] = '\0';
  //pointer = buf;
}*/

void clean(unsigned int begin, unsigned int end) {
  cursor_moveto(begin, 0);
  unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
  video_buf += 80 * 2 * begin;
  int i = 0;
  while (i < 80 * 2 * end) {
    *(video_buf + i) = '\0';
    i+=2;
  }
}

void info() {
  clean(10, 15);
  const char* author = "Created by Kuleeva Anna, group 4851003/90001, IBKS 2021.";
  const char* os = "OS: Linux";
  const char* tr = "Assemler: YASM, Intel syntax";
  const char* deb = "Debugger: gcc";
  const char* ltr = "You can use these letters:";

  out_str(0xDC, author, 10);
  out_str(0xDC, os, 11);
  out_str(0xDC, tr, 12);
  out_str(0xDC, deb, 13);
  out_str(0xDC, ltr, 14);
  out_str(0xDC, (const char*) alphabet, 15);
  pos = 0; //return carete for next command
}

unsigned char helper(unsigned int x) {
  switch (x) {
    case 0: return '0'; //break;
    case 1: return '1'; //break;
    case 2: return '2'; //break;
    case 3: return '3'; //break;
    case 4: return '4'; //break;
    case 5: return '5'; //break;
    case 6: return '6'; //break;
    case 7: return '7'; //break;
    case 8: return '8'; //break;
    case 9: return '9'; //break;
  }
}

void dictinfo() {
  clean(10, 15);
  const char* lang = "Language: en -> es";
  const char* all = "MAX words: 52";
  unsigned char load[20] = "Loaded words: ";

  if (loaded < 10)
    load[13] = helper(loaded);
  else {
    unsigned int copy = loaded;
    copy %= 10; //take first
    load[14] = helper(copy);
    copy = loaded;
    copy /= 10; //take second
    load[13] = helper(copy);
  }

  out_str(0xDC, lang, 10);
  out_str(0xDC, all, 11);
  out_str(0xDC, (const char*) load, 12);
  pos = 0; //return carete for next command
}

void wordstat(unsigned char c) {
  clean(10, 15);
  unsigned int i = 0;
    //out_str(0xDC, (const char*) c, 11);

  for (i = 0; i < 27; i++)
    if (alphabet[i] == c) {
      const char* yes = "There are 2 words on this letter";
      out_str(0xDC, yes, 10);
      break;
    }
  if (i == 27) {
    const char* no = "There are 0 words on this letter";
    out_str(0xDC, no, 10);
  }
  pos = 0; //return carete for next command
}

void translate(unsigned char *word) {
  clean(10, 15);
  //out_str(0xDC, (const char*) word, 14);
  unsigned int len = strlen((const char*) word);
  if (len == -1) {
    pos = 0; //return carete for next command
    return; //check arg
  }

  unsigned int i = 0;
  for (i = 0; i < 27; i++)
    if (alphabet[i] == word[0])
      break;

  if (i == 27) {
//    clean(10, 15);
    const char* no_let = "This letter wasn't loaded.";
    out_str(0xDC, no_let, 10);
    pos = 0; //return carete for next command
    return;
  } else { //letter was found
    i *= 2;
    if (strncmp(word, (unsigned char*) english[i], len) && len == strlen(english[i]))
      out_str(0xDC, spanish[i], 10);
    else {
      if (strncmp(word, (unsigned char*) english[i + 1], len) && len == strlen(english[i + 1]))
        out_str(0xDC, spanish[i + 1], 10);
      else {
        const char* no_word = "There are no such word in dictionary :(";
        out_str(0xDC, no_word, 10);
        //pos = 0; //return carete for next command
        //return;
      }
    }
  }
  pos = 0; //return carete for next command
}

void shutdown() {
  clean(10, 15);
  // This is the fix for Qemu support. 
  // Dmitry V. Reshetov, IKBS, SPbSTU, 2016
  //outw (0xB004, 0x2000); // qemu < 1.7, ex. 1.6.2

  unsigned int data = 0x2000;
  outw (0x604, data);  // qemu >= 1.7  
}

extern "C" int kmain() {
  /*short tmp; //get string from stack
  asm ("pop bx mov tmp, bx");
  *alphabet = (unsigned char*)tmp;*/

//  get_alfa(alphabet);

  //clean(0);

  unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR; //get alpha from loader
  for (unsigned int i = 0; i < 26; i++) { 
    alphabet[i] = (unsigned char)video_buf[0];
    video_buf += 2;
  }

  for (unsigned int i = 0; i < 27; i++) //count loaded letters
    if(alphabet[i] != ' ') loaded++;
  loaded--;
  loaded *= 2; //two words each letter

  const char* hello = "Welcome to DictOS (gcc edition)!";
  const char* invite = "Please choose command:";
  const char* inf = "info -- information about creator and development tools";
  const char* di = "dictinfo -- information about language and amount of words";
  const char* trans = "translate [word] -- translation from english";
  const char* ws = "wordstat [letter] -- amount of words on this letter";
  const char* sd = "shutdown -- power off computer";
  const char* kb = "Use Enter to put your arguments, Backspace to delete extra symbols, Alt to finish command and put next arguments.";
  // Вывод строки
  out_str(0xDC, hello, 0);
  out_str(0xDC, invite, 1);
  out_str(0xDC, inf, 2);
  out_str(0xDC, di, 3);
  out_str(0xDC, trans, 4);
  out_str(0xDC, ws, 5);
  out_str(0xDC, sd, 6);
  out_str(0xDC, kb, 7);

  cursor_moveto(9, 0); //user write from here, "hello" msg stay like helper

  intr_disable();
  intr_init(); //set interrupts (for keyboard)
  keyb_init(); //set up keyboard
  intr_start(); //reg desc
  intr_enable(); //start work with intr on
  // Бесконечный цикл
  while(1) {
    asm("hlt");
  }
  return 0;
}