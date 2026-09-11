/* utf8.h — минимум работы с UTF-8: длина точки, итерация, индексы.
 * Строки в GoidaLang — это байты; срезы и длина считаются в кодовых точках.
 */
#ifndef GOIDA_UTF8_H
#define GOIDA_UTF8_H

#include <stddef.h>
#include <stdint.h>

/* Декодирует одну кодовую точку. Возвращает число прочитанных байт
 * (0 — если последовательность повреждена). */
size_t utf8_decode(const char *s, size_t len, uint32_t *cp);

/* Длина кодирования символа по его первому байту (1..4, 0 = не UTF-8). */
size_t utf8_charlen(unsigned char c);

/* Может ли точка начинать идентификатор (буквы, '_', любой байт >= 0x80). */
int utf8_is_ident_start(const char *s, size_t len);
/* Может ли точка продолжать идентификатор (start + цифры). */
int utf8_is_ident_part(const char *s, size_t len);

/* Число кодовых точек. */
size_t utf8_strlen(const char *s, size_t len);
/* Байтовое смещение кодовой точки с индексом idx (или len, если idx за концом). */
size_t utf8_offset(const char *s, size_t len, size_t idx);
/* Кодирует точку в out (до 4 байт), возвращает длину. */
size_t utf8_encode(uint32_t cp, char *out);

#endif /* GOIDA_UTF8_H */
