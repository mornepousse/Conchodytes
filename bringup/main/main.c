/* Conchodytes — bring-up capteur PMW3389.
 *
 * Une seule question : est-ce que le capteur repond, et est-ce qu'il voit.
 *
 * LA PUCE MONTEE EST UN PMW3389DM-T3QU, PAS UN PMW3360. Product_ID lu sur la
 * carte le 2026-08-25 : 0x47, avec Revision_ID 0x01 — les deux valeurs par
 * defaut du 3389 (datasheet Version 1.0 | 07 sep 2017, p. 20). Tout le depot
 * (README, BOM, NOTES-V2.md, symbole KiCad U2) parle encore du 3360 : c'est le
 * depot qui a tort. Les deux puces ne partagent PAS leur blob SROM ni tout
 * leur plan de registres.
 * Pas de radio, pas de HID, pas de keymap. Sortie sur UART0 (J1).
 *
 * Brochage releve a la netlist de hardware/pcb/ au kicad-cli :
 *   SCK_L  = GPIO38   MISO_L = GPIO39   MOSI_L = GPIO40   (100 R en serie)
 *   SNS_NCS = GPIO17  SNS_MOTION = GPIO18
 *   nRF24 : CSN = GPIO2, CE = GPIO1  -> maintenus inactifs, bus partage.
 *
 * Datasheet PMW3389DM-T3QU, PixArt, Version 1.0 | 07 sep 2017 :
 *   Product_ID (0x00)         = 0x47   p. 20
 *   Revision_ID (0x01)        = 0x01   p. 20
 *   Inverse_Product_ID (0x3F) = 0xB9 annonce p. 20, mais 0x47 ^ 0xFF = 0xB8 et
 *     c'est 0xB8 que la carte renvoie. Coquille de la datasheet ; on attend le
 *     complement exact, qui est en plus une verification de bus gratuite.
 *   VDD 1,8-2,1 V / VDDIO 1,8-3,6 V     p. 1 et Table 4, p. 15
 *   Resolution sur 0x0E/0x0F (16 bits), et NON sur un Config1 8 bits comme le
 *     3360. Ce bring-up n'y touche pas : les valeurs de reset suffisent pour
 *     savoir si la puce detecte du mouvement, et ca supprime une inconnue.
 *
 * ATTENTION MATERIEL : sur la v1 sans rework, U100 est cable a l'envers
 * (pad 2 = VOUT sur +3V3, pad 3 = VIN sur le rail capteur). La diode de
 * substrat du PMOS pose le rail a ~2,7 V, au-dessus des 2,10 V de la plage
 * d'emploi (Table 4, p. 15). Deposer U100 et injecter 1,9 V sur C100/C101/C103 avant
 * d'accorder le moindre credit a ce que ce programme affiche.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "conch";

/* ── Brochage ───────────────────────────────────────────────────────────── */
#define PIN_SCK        GPIO_NUM_38
#define PIN_MISO       GPIO_NUM_39
#define PIN_MOSI       GPIO_NUM_40
#define PIN_SNS_NCS    GPIO_NUM_17
#define PIN_SNS_MOTION GPIO_NUM_18
#define PIN_NRF_CSN    GPIO_NUM_2
#define PIN_NRF_CE     GPIO_NUM_1
#define PIN_NRF_IRQ    GPIO_NUM_41

/* Clics : trois SPDT, COM a la masse, NO et NC tires chacun au 3,3 V par 10 k
 * (R105-R107 et R108-R110). ATTENTION : le README annoncait LEFT_NC sur GPIO4
 * et RIGHT_NC sur GPIO5 — c'est l'inverse. Valeurs ci-dessous relevees a la
 * netlist : GPIO4 va a SW2.3 (clic DROIT), GPIO5 a SW1.3 (clic GAUCHE). */
#define PIN_SW_LEFT       GPIO_NUM_10
#define PIN_SW_LEFT_NC    GPIO_NUM_5
#define PIN_SW_RIGHT      GPIO_NUM_11
#define PIN_SW_RIGHT_NC   GPIO_NUM_4
#define PIN_SW_MID        GPIO_NUM_12
#define PIN_SW_MID_NC     GPIO_NUM_6

/* Molette : encodeur optique, LD1 eclaire une roue a 60 fentes, LQ1 double
 * phototransistor -> deux voies en quadrature. */
#define PIN_ENC_A      GPIO_NUM_7
#define PIN_ENC_B      GPIO_NUM_9

#define SNS_SPI_HOST   SPI2_HOST
#define SNS_CLOCK_HZ   (2 * 1000 * 1000)   /* fSCLK max = 2,0 MHz, Table 4, p. 15 */

/* ── Registres, p. 20 ───────────────────────────────────────────────────── */
#define REG_Product_ID         0x00
#define REG_Revision_ID        0x01
#define REG_Motion             0x02
#define REG_Delta_X_L          0x03
#define REG_Delta_X_H          0x04
#define REG_Delta_Y_L          0x05
#define REG_Delta_Y_H          0x06
#define REG_SQUAL              0x07
#define REG_Shutter_Lower      0x0B
#define REG_Shutter_Upper      0x0C
#define REG_Resolution_L       0x0E   /* 3389 : 16 bits, pas le Config1 du 3360 */
#define REG_Resolution_H       0x0F
#define REG_Config2            0x10
#define REG_SROM_Enable        0x13
#define REG_SROM_ID            0x2A
#define REG_Power_Up_Reset     0x3A
#define REG_Inverse_Product_ID 0x3F
#define REG_Motion_Burst       0x50
#define REG_SROM_Load_Burst    0x62

extern const unsigned short firmware_length;
extern const unsigned char firmware_data[];

static spi_device_handle_t s_sns;

/* ── Transport SPI, CS pilote a la main ─────────────────────────────────────
 * Le CS materiel d'ESP-IDF le releverait entre l'octet d'adresse et l'octet de
 * donnee, or tSRAD tombe justement dans cet intervalle. D'ou spics_io_num = -1
 * et ces deux fonctions. Meme geste que main/comm/rf/rf_driver.c cote KeSp. */

static inline void cs_low(void)  { gpio_set_level(PIN_SNS_NCS, 0); esp_rom_delay_us(1); }
static inline void cs_high(void) { esp_rom_delay_us(1); gpio_set_level(PIN_SNS_NCS, 1); }

static void spi_tx(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_sns, &t));
}

static uint8_t reg_read(uint8_t addr)
{
    uint8_t a = addr & 0x7F;      /* MSB a 0 = lecture */
    uint8_t d = 0;

    cs_low();
    spi_tx(&a, NULL, 1);
    esp_rom_delay_us(180);        /* tSRAD = 160 us MINIMUM sur le 3389 (Table 5,
                                   * p. 16) — contre 35 us sur le 3360. Le code de
                                   * reference d'ou vient ce driver attend 100 us :
                                   * hors spec ici, et c'est le genre d'ecart qui
                                   * passe au banc puis lache par intermittence. */
    spi_tx(NULL, &d, 1);
    cs_high();
    esp_rom_delay_us(20);         /* tSRW/tSRR = 20 us (Table 5, p. 16) */
    return d;
}

static void reg_write(uint8_t addr, uint8_t val)
{
    uint8_t buf[2] = { (uint8_t)(addr | 0x80), val };   /* MSB a 1 = ecriture */

    cs_low();
    spi_tx(buf, NULL, 2);
    esp_rom_delay_us(20);         /* tSCLK-NCS en ecriture */
    cs_high();
    esp_rom_delay_us(200);        /* tSWW/tSWR = 180 us mini sur le 3389 (p. 16) */
}

/* ── Televersement SROM ─────────────────────────────────────────────────────
 * 4094 octets a 15 us l'octet : ~61 ms pendant lesquelles rien d'autre ne doit
 * toucher le bus. acquire_bus le garantit face au nRF24. */
static void srom_upload(void)
{
    reg_write(REG_Config2, 0x20);        /* valeur par defaut du registre, p. 20 */
    reg_write(REG_SROM_Enable, 0x1D);
    vTaskDelay(pdMS_TO_TICKS(10));       /* plus d'une periode de trame */
    reg_write(REG_SROM_Enable, 0x18);

    uint8_t burst = REG_SROM_Load_Burst | 0x80;
    cs_low();
    spi_tx(&burst, NULL, 1);
    esp_rom_delay_us(15);
    for (unsigned i = 0; i < firmware_length; i++) {
        spi_tx(&firmware_data[i], NULL, 1);
        esp_rom_delay_us(15);
    }
    cs_high();
    esp_rom_delay_us(200);

    uint8_t srom_id = reg_read(REG_SROM_ID);
    ESP_LOGI(TAG, "SROM_ID = 0x%02X  (0x00 = televersement rate)", srom_id);

    /* 0x00 = mode filaire, Rest desactive : au banc on ne veut pas que la puce
     * s'endorme entre deux mesures. La resolution reste a sa valeur de reset. */
    reg_write(REG_Config2, 0x00);
}

/* Remise a zero du port serie du capteur, PUIS reset logiciel.
 *
 * A faire AVANT la toute premiere lecture de registre, et pas apres.
 * Constate sur carte le 2026-08-25 : en lisant Product_ID avant cette
 * sequence, la puce renvoyait 0x11 la ou elle vaut 0x47 — soit 0x47 >> 2,
 * un decalage de deux bits d'horloge. Les lectures suivantes etaient
 * alignees. La machine a etats SPI du capteur n'est pas dans un etat connu
 * a la mise sous tension ; le triple battement de NCS l'y remet. */
static void sensor_reset(void)
{
    cs_high(); cs_low(); cs_high();      /* remise a zero du port serie */
    reg_write(REG_Power_Up_Reset, 0x5A);
    vTaskDelay(pdMS_TO_TICKS(50));       /* tMOT-RST = 50 ms, Table 5, p. 16 */

    /* lire et jeter 0x02..0x06 apres le reset */
    (void)reg_read(REG_Motion);
    (void)reg_read(REG_Delta_X_L);
    (void)reg_read(REG_Delta_X_H);
    (void)reg_read(REG_Delta_Y_L);
    (void)reg_read(REG_Delta_Y_H);
}

static void sensor_load_srom(void)
{
    ESP_ERROR_CHECK(spi_device_acquire_bus(s_sns, portMAX_DELAY));
    srom_upload();
    spi_device_release_bus(s_sns);

    vTaskDelay(pdMS_TO_TICKS(10));
}

/* ── Lecture du mouvement ───────────────────────────────────────────────── */
static void motion_read(int16_t *dx, int16_t *dy, uint8_t *squal)
{
    reg_write(REG_Motion, 0x01);         /* fige les compteurs */
    (void)reg_read(REG_Motion);

    uint8_t xl = reg_read(REG_Delta_X_L), xh = reg_read(REG_Delta_X_H);
    uint8_t yl = reg_read(REG_Delta_Y_L), yh = reg_read(REG_Delta_Y_H);

    *dx = (int16_t)(((uint16_t)xh << 8) | xl);
    *dy = (int16_t)(((uint16_t)yh << 8) | yl);
    *squal = reg_read(REG_SQUAL);
}

/* ── Etablir la disposition du Motion_Burst, par la mesure ──────────────────
 *
 * Le burst (registre 0x50) rend plusieurs octets en UNE transaction : une seule
 * adresse, un seul tSRAD — et en mode burst il vaut 35 us au lieu de 160
 * (Table 5, p. 16). Un releve complet passe de ~1,9 ms a ~90 us.
 *
 * Probleme : NI la datasheet du 3389 (20 pages) NI celle du 3360 ne decrivent
 * la disposition des octets. Elle circule dans les implementations
 * communautaires, mais on ne la recopie pas sans preuve.
 *
 * Methode : afficher cote a cote les octets du burst et les memes grandeurs
 * lues registre par registre. Au repos, SQUAL et Shutter sont stables et
 * identifiables ; en bougeant la souris, les octets Delta se mettent a varier.
 * Deux captures suffisent a etablir la correspondance par observation.
 *
 * Procedure du burst :
 *   1. ecrire n'importe quoi dans Motion_Burst (0x50) pour l'armer
 *   2. NCS bas, envoyer 0x50 (adresse seule, pas de bit d'ecriture)
 *   3. attendre tSRAD_MOTBR = 35 us
 *   4. lire N octets d'affilee, sans relever NCS
 *   5. NCS haut, puis tBEXIT = 500 ns avant toute autre transaction SPI
 */
#define BURST_N 16

static void burst_read(uint8_t *out, size_t n)
{
    reg_write(REG_Motion_Burst, 0x00);      /* arme le mode burst */

    uint8_t addr = REG_Motion_Burst;        /* MSB a 0 : lecture */
    cs_low();
    spi_tx(&addr, NULL, 1);
    esp_rom_delay_us(35);                   /* tSRAD_MOTBR, Table 5 p. 16 */
    spi_tx(NULL, out, n);                   /* n octets d'affilee */
    cs_high();
    esp_rom_delay_us(5);                    /* tBEXIT = 500 ns, large */
}

static void burst_compare(void)
{
    uint8_t b[BURST_N];
    burst_read(b, BURST_N);

    /* Reference : les memes grandeurs, lues une par une juste apres. */
    uint8_t mot = reg_read(REG_Motion);
    uint8_t xl = reg_read(REG_Delta_X_L), xh = reg_read(REG_Delta_X_H);
    uint8_t yl = reg_read(REG_Delta_Y_L), yh = reg_read(REG_Delta_Y_H);
    uint8_t sq = reg_read(REG_SQUAL);
    uint8_t sl = reg_read(REG_Shutter_Lower), sh = reg_read(REG_Shutter_Upper);

    char l[BURST_N * 3 + 1]; int n = 0;
    for (unsigned i = 0; i < BURST_N; i++)
        n += snprintf(l + n, sizeof(l) - n, "%02X ", b[i]);

    ESP_LOGI(TAG, "burst[%2d] %s", BURST_N, l);
    ESP_LOGI(TAG, "   refs   Motion=%02X dXl=%02X dXh=%02X dYl=%02X dYh=%02X "
                  "SQUAL=%02X Shl=%02X Shh=%02X",
             mot, xl, xh, yl, yh, sq, sl, sh);
}

/* ── Diagnostic : SQUAL est-il vraiment bruite, ou est-ce ma lecture ? ──────
 *
 * Au banc du 2026-08-25, SQUAL alternait entre ~15 et ~80 d'un echantillon a
 * l'autre alors que Shutter restait stable a ~47. Un obturateur stable veut dire
 * un eclairement et une mise au point stables : le probleme n'est donc pas
 * optique, ou alors il faudrait qu'il le soit sans toucher a la quantite de
 * lumiere, ce qui n'a pas de sens.
 *
 * L'autre suspect est la boucle de lecture elle-meme, qui fige les compteurs par
 * reg_write(Motion, 0x01) avant de lire. Cette rafale lit SQUAL SEUL, sans
 * toucher a Motion, a 5 ms d'intervalle. Si la dispersion disparait, c'etait
 * bien la sequence de lecture ; si elle reste, c'est la surface ou l'optique. */
static void squal_probe(void)
{
    enum { N = 40 };
    uint8_t v[N];
    for (int i = 0; i < N; i++) { v[i] = reg_read(REG_SQUAL); vTaskDelay(pdMS_TO_TICKS(5)); }

    unsigned mn = 255, mx = 0, som = 0;
    char ligne[N * 4 + 1]; int n = 0;
    for (int i = 0; i < N; i++) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
        som += v[i];
        n += snprintf(ligne + n, sizeof(ligne) - n, "%u ", v[i]);
    }
    ESP_LOGI(TAG, "SQUAL x%d en rafale : %s", N, ligne);
    ESP_LOGI(TAG, "SQUAL min=%u max=%u moy=%u  (etendue %u)", mn, mx, som / N, mx - mn);
    ESP_LOGI(TAG, "  etendue faible => c'etait ma sequence de lecture.");
    ESP_LOGI(TAG, "  etendue large  => surface ou optique. Compare sur tapis vs bureau nu.");
}

/* ── Clics : anti-rebond par le contact NC, sans filtrage temporel ──────────
 *
 * Un SPDT ne rebondit pas "entre deux etats logiques" : pendant le rebond le
 * contact mobile n'est colle NI sur NO NI sur NC, donc les deux entrees sont
 * hautes (tirees par leurs 10 k). C'est un etat OBSERVABLE, et il suffit d'y
 * garder l'etat precedent pour supprimer le double-clic — aucun compteur, aucun
 * delai, aucune constante a regler.
 *
 *   repos    : NC ferme sur COM -> bas ; NO ouvert -> haut
 *   appuye   : NO ferme sur COM -> bas ; NC ouvert -> haut
 *   rebond   : les DEUX hauts               -> on ne conclut rien
 *   les deux bas : impossible physiquement  -> on ne conclut rien non plus
 */
typedef struct {
    gpio_num_t no, nc;
    bool       pressed;
    const char *nom;
    uint32_t   fronts;    /* transitions retenues */
    uint32_t   rebonds;   /* echantillons "les deux hauts" = rebond en cours */
    uint32_t   rebond_max;/* plus longue salve de rebond, en ms */
    uint32_t   rebond_cur;
} clic_t;

static clic_t s_clics[3] = {
    { PIN_SW_LEFT,  PIN_SW_LEFT_NC,  false, "G", 0, 0, 0, 0 },
    { PIN_SW_RIGHT, PIN_SW_RIGHT_NC, false, "D", 0, 0, 0, 0 },
    { PIN_SW_MID,   PIN_SW_MID_NC,   false, "M", 0, 0, 0, 0 },
};

/* Rend true si l'etat a change. Appelee a 1 kHz : sans ca la fenetre de rebond,
 * qui dure quelques millisecondes, passe entre deux echantillons et on ne peut
 * rien affirmer sur l'anti-rebond. */
static bool clic_update(clic_t *c)
{
    int no = gpio_get_level(c->no);
    int nc = gpio_get_level(c->nc);

    if (no == 1 && nc == 1) {          /* contact en l'air : rebond en cours */
        c->rebonds++;
        if (++c->rebond_cur > c->rebond_max) c->rebond_max = c->rebond_cur;
        return false;                  /* on garde l'etat precedent */
    }
    c->rebond_cur = 0;

    if (no == 0 && nc == 1) {          /* NO colle sur COM : appuye */
        bool change = !c->pressed;
        c->pressed = true;
        if (change) c->fronts++;
        return change;
    }
    if (no == 1 && nc == 0) {          /* NC colle sur COM : repos */
        bool change = c->pressed;
        c->pressed = false;
        if (change) c->fronts++;
        return change;
    }
    return false;                      /* les deux bas : impossible, on ignore */
}

/* ── Molette : quadrature ───────────────────────────────────────────────────
 * Table de transition classique indexee par (precedent << 2) | courant. Les
 * transitions impossibles — les deux voies changeant dans le meme intervalle —
 * rendent 0 plutot qu'un pas invente. */
static const int8_t QUAD[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

static volatile int32_t s_wheel;      /* comptage brut, en pas de quadrature */
static volatile uint32_t s_wheel_err; /* transitions impossibles vues */

static void wheel_task(void *arg)
{
    (void)arg;
    uint8_t prev = (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) | gpio_get_level(PIN_ENC_B));
    while (1) {
        uint8_t cur = (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) | gpio_get_level(PIN_ENC_B));
        if (cur != prev) {
            int8_t pas = QUAD[(prev << 2) | cur];
            if (pas) s_wheel += pas;
            else     s_wheel_err++;   /* saut de deux etats : pas rate */
            prev = cur;
        }

        /* Les clics sont scrutes ici, dans la meme tache a 1 kHz — c'est la
         * seule cadence a laquelle le rebond est observable. */
        for (int i = 0; i < 3; i++) {
            if (clic_update(&s_clics[i]))
                ESP_LOGW(TAG, "clic %s : %s", s_clics[i].nom,
                         s_clics[i].pressed ? "APPUYE" : "relache");
        }

        vTaskDelay(1);               /* 1 tick = 1 ms au reglage par defaut */
    }
}

/* ── Diagnostic molette : court franc, ou transistor qui conduit ? ──────────
 *
 * ENC_A/ENC_B restent a 0 quoi qu'on fasse : opaque entre LD1 et LQ1, tirage
 * externe 10 k, tirage interne 45 k. Reste a savoir CE qui les tient.
 *
 * On pilote chaque ligne en sortie et on relit. Force de sortie au minimum
 * (~5 mA) : meme sur un court franc a la masse, c'est sans danger pour le S3.
 *
 *   relit 1 -> pas de court franc. Quelque chose de MOU tire au bas : un
 *              phototransistor qui conduit, donc LQ1 voit de la lumiere ou est
 *              mal monte. La sortie du S3 le domine.
 *   relit 0 -> court FRANC a la masse. Pont de soudure, ou LQ1 interne en
 *              court. Le fer est necessaire.
 */
static void enc_drive_test(void)
{
    /* GPIO8 (broche 12 du module) est NON CONNECTE sur cette carte d'apres la
     * netlist : il sert de temoin. S'il ne se pilote pas non plus, c'est la
     * methode de mesure qui est fausse et pas le cablage. */
    const struct { gpio_num_t p; const char *n; } lignes[] = {
        { PIN_ENC_A,   "ENC_A  (GPIO7) " },
        { PIN_ENC_B,   "ENC_B  (GPIO9) " },
        /* Deux temoins, et le second vaut mieux que le premier :
         *   GPIO8 est NON CONNECTE — il ne prouve que le cas facile ;
         *   SW_LEFT porte un vrai tirage 10 k et un interrupteur vers la masse,
         *   soit exactement la topologie de ENC_A/ENC_B. S'il se pilote et pas
         *   eux, la difference n'est pas dans la methode. */
        { GPIO_NUM_8,  "TEMOIN libre   " },
        { PIN_SW_LEFT, "TEMOIN charge  " },
    };
    const gpio_drive_cap_t caps[] = {
        GPIO_DRIVE_CAP_0, GPIO_DRIVE_CAP_1, GPIO_DRIVE_CAP_2, GPIO_DRIVE_CAP_3,
    };

    for (unsigned i = 0; i < 4; i++) {
        gpio_num_t g = lignes[i].p;
        int lu[4];

        ESP_ERROR_CHECK(gpio_set_direction(g, GPIO_MODE_INPUT_OUTPUT));
        for (unsigned c = 0; c < 4; c++) {
            ESP_ERROR_CHECK(gpio_set_drive_capability(g, caps[c]));
            gpio_set_level(g, 1);
            esp_rom_delay_us(500);
            lu[c] = gpio_get_level(g);
        }
        gpio_set_level(g, 0);
        ESP_ERROR_CHECK(gpio_set_drive_capability(g, GPIO_DRIVE_CAP_2));
        ESP_ERROR_CHECK(gpio_set_direction(g, GPIO_MODE_INPUT));

        ESP_LOGW(TAG, "%s pilotee HAUT, force 0/1/2/3 -> %d %d %d %d  => %s",
                 lignes[i].n, lu[0], lu[1], lu[2], lu[3],
                 lu[3] ? "la ligne remonte" : "tenue au BAS meme a pleine force");
        /* Repassee en entree juste au-dessus ; SW_LEFT doit le rester, le
         * scrutin des clics le lit. */
    }
}

static void input_init(void)
{
    /* Les tirages sont EXTERNES (10 k) sur les six contacts comme sur les deux
     * voies de l'encodeur — on n'arme pas ceux du S3, qui les mettraient en
     * parallele et fausseraient les seuils. */
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_SW_LEFT)  | (1ULL << PIN_SW_LEFT_NC)  |
                        (1ULL << PIN_SW_RIGHT) | (1ULL << PIN_SW_RIGHT_NC) |
                        (1ULL << PIN_SW_MID)   | (1ULL << PIN_SW_MID_NC)   |
                        (1ULL << PIN_ENC_A)    | (1ULL << PIN_ENC_B),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    enc_drive_test();

    for (int i = 0; i < 3; i++) (void)clic_update(&s_clics[i]);

    xTaskCreate(wheel_task, "wheel", 2560, NULL, 10, NULL);
}

/* ── Sonde nRF24L01+ ────────────────────────────────────────────────────────
 *
 * Le module partage le bus avec le capteur, mais en mode 0 (le PMW3389 est en
 * mode 3) et jusqu'a 10 MHz. ESP-IDF reconfigure le mode par appareil ; c'est
 * l'exclusion des CS qui reste a notre charge, et elle est deja tenue.
 *
 * Le nRF24 n'a pas de registre d'identite. La verification equivalente est
 * l'aller-retour : ecrire une valeur inhabituelle dans un registre inoffensif
 * et la relire. Si elle revient, le module est monte, alimente, et les six
 * lignes (SCK, MOSI, MISO, CSN, plus l'alimentation) sont bonnes. Si on lit
 * 0x00 ou 0xFF, MISO ne parle pas.
 */
#define NRF_R_REGISTER   0x00
#define NRF_W_REGISTER   0x20
#define NRF_REG_CONFIG   0x00
#define NRF_REG_EN_AA    0x01
#define NRF_REG_SETUP_AW 0x03
#define NRF_REG_RF_CH    0x05
#define NRF_REG_RF_SETUP 0x06

static spi_device_handle_t s_nrf;

static inline void nrf_cs(int lvl) { gpio_set_level(PIN_NRF_CSN, lvl); esp_rom_delay_us(1); }

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(NRF_R_REGISTER | (reg & 0x1F)), 0xFF };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    nrf_cs(0);
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_nrf, &t));
    nrf_cs(1);
    return rx[1];
}

static void nrf_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(NRF_W_REGISTER | (reg & 0x1F)), val };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    nrf_cs(0);
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_nrf, &t));
    nrf_cs(1);
}

/* Trois suspects quand le module est en place et muet : CSN, l'alimentation,
 * ou le module. Ces sondes les departagent sans fer.
 *
 * IRQ est la plus parlante et elle est gratuite : c'est une SORTIE du nRF24,
 * active basse, donc HAUTE au repos. Si le module est alimente et vivant,
 * GPIO41 lit 1. S'il lit 0 ou flotte, l'alimentation ou le module sont en
 * cause, et CSN n'y est pour rien.
 *
 * CSN est teste dans les DEUX sens : une ligne qui ne peut pas descendre
 * n'active jamais le module, et MISO reste alors en haute impedance — ce qui
 * donne exactement les 0xFF qu'on lit. */
static void nrf_lines_probe(void)
{
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_NRF_IRQ,
                         .mode = GPIO_MODE_INPUT };
    ESP_ERROR_CHECK(gpio_config(&in));
    gpio_set_pull_mode(PIN_NRF_IRQ, GPIO_FLOATING);
    esp_rom_delay_us(500);
    int irq_libre = gpio_get_level(PIN_NRF_IRQ);
    gpio_set_pull_mode(PIN_NRF_IRQ, GPIO_PULLUP_ONLY);
    esp_rom_delay_us(500);
    int irq_tire = gpio_get_level(PIN_NRF_IRQ);
    gpio_set_pull_mode(PIN_NRF_IRQ, GPIO_FLOATING);

    ESP_LOGW(TAG, "nRF IRQ (GPIO%d) : sans tirage -> %d | avec tirage interne -> %d",
             PIN_NRF_IRQ, irq_libre, irq_tire);
    if (irq_tire)
        ESP_LOGE(TAG, "  => la ligne FLOTTAIT : rien ne la pilote. Le module n'est "
                      "pas alimente, ou pas en contact.");
    else
        ESP_LOGE(TAG, "  => quelque chose la TIENT au bas malgre le tirage : le "
                      "module est bien la, mais dans un etat anormal.");

    const struct { gpio_num_t p; const char *n; } l[] = {
        { PIN_NRF_CSN, "CSN (GPIO2)" }, { PIN_NRF_CE, "CE  (GPIO1)" },
    };
    for (unsigned i = 0; i < 2; i++) {
        ESP_ERROR_CHECK(gpio_set_direction(l[i].p, GPIO_MODE_INPUT_OUTPUT));
        gpio_set_level(l[i].p, 1); esp_rom_delay_us(200);
        int h = gpio_get_level(l[i].p);
        gpio_set_level(l[i].p, 0); esp_rom_delay_us(200);
        int b = gpio_get_level(l[i].p);
        gpio_set_level(l[i].p, 1);
        ESP_LOGW(TAG, "nRF %s : haut->%d  bas->%d  => %s", l[i].n, h, b,
                 (h == 1 && b == 0) ? "pilotable dans les deux sens"
                                    : "BLOQUEE — le module ne sera jamais selectionne");
    }
    gpio_set_level(PIN_NRF_CSN, 1);
    gpio_set_level(PIN_NRF_CE, 0);
}

static void nrf_probe(void)
{
    nrf_lines_probe();

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode           = 0,        /* le nRF24 est en mode 0, le capteur en 3 */
        .spics_io_num   = -1,       /* CSN manuel, comme pour le capteur */
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SNS_SPI_HOST, &dev, &s_nrf));

    uint8_t cfg = nrf_read_reg(NRF_REG_CONFIG);
    uint8_t aa  = nrf_read_reg(NRF_REG_EN_AA);
    uint8_t aw  = nrf_read_reg(NRF_REG_SETUP_AW);
    uint8_t ch  = nrf_read_reg(NRF_REG_RF_CH);
    uint8_t rf  = nrf_read_reg(NRF_REG_RF_SETUP);

    ESP_LOGI(TAG, "nRF24 CONFIG=%02X EN_AA=%02X SETUP_AW=%02X RF_CH=%02X RF_SETUP=%02X",
             cfg, aa, aw, ch, rf);
    ESP_LOGI(TAG, "  valeurs de reset attendues : 08   3F       03       02       0E");

    /* L'aller-retour, sur RF_CH : 0x52 est le canal du slot souris annonce par
     * boards/kase_dongle/board.h. On le laisse en place, il servira. */
    nrf_write_reg(NRF_REG_RF_CH, 0x52);
    uint8_t relu = nrf_read_reg(NRF_REG_RF_CH);
    ESP_LOGI(TAG, "  ecrit RF_CH=0x52 -> relu 0x%02X", relu);

    if (cfg == 0x00 && aa == 0x00 && ch == 0x00)
        ESP_LOGE(TAG, "=> tout a zero : MISO muet. Module absent, non alimente, "
                      "ou CSN mal pilote.");
    else if (cfg == 0xFF && aa == 0xFF)
        ESP_LOGE(TAG, "=> tout a un : MISO tire au haut, personne ne repond.");
    else if (relu != 0x52)
        ESP_LOGE(TAG, "=> lecture plausible mais l'ecriture ne prend pas (relu "
                      "0x%02X) : verifier MOSI et CSN.", relu);
    else
        ESP_LOGI(TAG, "=> le nRF24 repond et accepte les ecritures. Radio saine.");
}

/* ── Mise en place ──────────────────────────────────────────────────────── */
static void bus_init(void)
{
    /* Le nRF24 partage SCK/MOSI/MISO. Ses lignes sont posees inactives AVANT
     * que quoi que ce soit parle sur le bus : un CSN flottant pendant que le
     * capteur repond, c'est deux esclaves qui tirent MISO. */
    gpio_config_t idle = {
        .pin_bit_mask = (1ULL << PIN_NRF_CSN) | (1ULL << PIN_NRF_CE) |
                        (1ULL << PIN_SNS_NCS),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&idle));
    gpio_set_level(PIN_NRF_CSN, 1);      /* CSN inactif = haut */
    gpio_set_level(PIN_NRF_CE,  0);      /* radio au repos */
    gpio_set_level(PIN_SNS_NCS, 1);

    gpio_config_t motion = {
        .pin_bit_mask = 1ULL << PIN_SNS_MOTION,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&motion));

    spi_bus_config_t bus = {
        .sclk_io_num     = PIN_SCK,
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SNS_SPI_HOST, &bus, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = SNS_CLOCK_HZ,
        .mode           = 3,             /* PMW3360 : CPOL=1, CPHA=1 */
        .spics_io_num   = -1,            /* CS manuel, cf. plus haut */
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SNS_SPI_HOST, &dev, &s_sns));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Conchodytes bring-up capteur — PMW3389");
    ESP_LOGI(TAG, "SCK=%d MISO=%d MOSI=%d NCS=%d MOTION=%d @ %d Hz mode 3",
             PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SNS_NCS, PIN_SNS_MOTION, SNS_CLOCK_HZ);

    bus_init();
    vTaskDelay(pdMS_TO_TICKS(50));

    sensor_reset();

    /* L'identite APRES le reset mais AVANT le SROM : le reset garantit que le
     * port serie est aligne, et lire l'identite d'abord evite qu'un
     * televersement rate masque une puce qui ne repondait deja pas. */
    uint8_t id  = reg_read(REG_Product_ID);
    uint8_t inv = reg_read(REG_Inverse_Product_ID);
    uint8_t rev = reg_read(REG_Revision_ID);

    ESP_LOGI(TAG, "Product_ID = 0x%02X   (attendu 0x47, p. 20)", id);
    ESP_LOGI(TAG, "Inverse    = 0x%02X   (attendu 0xB8)", inv);
    ESP_LOGI(TAG, "Revision   = 0x%02X", rev);

    if (id == 0x00 && inv == 0x00) {
        ESP_LOGE(TAG, "tout a zero : MISO muet. Capteur non alimente, NCS mal");
        ESP_LOGE(TAG, "pilote, ou ligne coupee. U100 est-il depose et le rail");
        ESP_LOGE(TAG, "injecte a 1,9 V sur C100/C101/C103 ?");
    } else if (id == 0xFF && inv == 0xFF) {
        ESP_LOGE(TAG, "tout a un : MISO tire au haut, personne ne repond.");
    } else if ((id ^ inv) != 0xFF) {
        ESP_LOGE(TAG, "0x%02X ^ 0x%02X != 0xFF : le bus ment. Mode SPI, horloge,",
                 id, inv);
        ESP_LOGE(TAG, "ou le nRF24 qui repond a la place du capteur.");
    } else if (id != 0x47) {
        ESP_LOGW(TAG, "complement coherent mais 0x%02X n'est pas un PMW3389.", id);
    } else {
        ESP_LOGI(TAG, "=> PMW3389 confirme. Televersement du SROM.");
        sensor_load_srom();
    }

    /* Boucle de banc : mouvement, qualite d'image et obturateur. SQUAL compte
     * les points de contraste vus ; a zero avec un Shutter au plafond, le
     * capteur est aveugle — c'est la signature de D100 montee a l'envers, qui
     * empeche la LED interne de s'allumer (NOTES-V2.md point 6). */
    if (id == 0x47) squal_probe();

    nrf_probe();

    input_init();
    ESP_LOGI(TAG, "clics G=%d/%d D=%d/%d M=%d/%d (NO/NC), molette A=%d B=%d",
             PIN_SW_LEFT, PIN_SW_LEFT_NC, PIN_SW_RIGHT, PIN_SW_RIGHT_NC,
             PIN_SW_MID, PIN_SW_MID_NC, PIN_ENC_A, PIN_ENC_B);

    int32_t wheel_prev = 0;
    unsigned tour = 0;
    /* Le burst consomme les compteurs a chaque lecture : a 1 kHz chaque releve
     * ne vaut qu'une milliseconde de mouvement. On accumule avant d'afficher,
     * sinon un balayage franc se dissout en miettes indistinguables. */
    int32_t acc23 = 0, acc45 = 0; unsigned nacc = 0;

    /* Diagnostic molette : ENC_A/ENC_B restent a 00 meme avec un opaque entre
     * LD1 et LQ1, donc rien d'optique. Or on compte sur les 10 k EXTERNES
     * (R103/R104) et les tirages internes sont desactives. On les bascule
     * toutes les 5 s :
     *   passe a 11 avec tirage interne -> R103/R104 ne tirent pas (non montees,
     *     soudure froide, net coupe) ; le S3 peut prendre le relais.
     *   reste a 00 avec tirage interne -> quelque chose tire activement au bas,
     *     LQ1 en court ou pont de soudure. Le ~45 k interne est faible, mais il
     *     suffit a departager une ligne flottante d'une ligne tenue. */
    bool pu = false;
    while (1) {
        /* motion_read() est DESACTIVE pendant la campagne d'axes : il fige et
         * vide les compteurs de deplacement, donc il volait au burst le
         * mouvement qu'on cherche a mesurer. Les deux ne peuvent pas coexister
         * dans la meme boucle — c'est d'ailleurs l'argument pour ne garder que
         * le burst une fois sa disposition etablie. */
        int16_t dx = 0, dy = 0;
        uint8_t squal = reg_read(REG_SQUAL);

        uint8_t sl = reg_read(REG_Shutter_Lower);
        uint8_t sh = reg_read(REG_Shutter_Upper);

        int32_t w = s_wheel, dw = w - wheel_prev;
        wheel_prev = w;

        /* Niveaux bruts NO/NC en plus de l'etat decode : si un clic n'apparait
         * pas dans G/D/M alors que ses deux broches bougent, c'est la logique
         * qui est fausse et pas le cablage — et reciproquement. Au repos on
         * attend NO=1 NC=0 sur les trois. */
        ESP_LOGI(TAG, "dx=%6d dy=%6d SQ=%3u Sh=%4u MOT=%d | G%d D%d M%d | "
                      "brut G%d%d D%d%d M%d%d | AB=%d%d molette %+4ld (tot %6ld, rat %lu)",
                 dx, dy, squal, ((unsigned)sh << 8) | sl,
                 gpio_get_level(PIN_SNS_MOTION),
                 s_clics[0].pressed, s_clics[1].pressed, s_clics[2].pressed,
                 gpio_get_level(PIN_SW_LEFT),  gpio_get_level(PIN_SW_LEFT_NC),
                 gpio_get_level(PIN_SW_RIGHT), gpio_get_level(PIN_SW_RIGHT_NC),
                 gpio_get_level(PIN_SW_MID),   gpio_get_level(PIN_SW_MID_NC),
                 gpio_get_level(PIN_ENC_A),    gpio_get_level(PIN_ENC_B),
                 (long)dw, (long)w, (unsigned long)s_wheel_err);

        /* Octets 2-5 du burst, decodes sous les deux hypotheses. Bouger sur un
         * seul axe a la fois departage : si l'axe immobile reste a 0,
         * l'hypothese correspondante est la bonne. */
        if (id == 0x47) {
            uint8_t bb[12];
            burst_read(bb, 12);
            acc23 += (int16_t)(((uint16_t)bb[3] << 8) | bb[2]);
            acc45 += (int16_t)(((uint16_t)bb[5] << 8) | bb[4]);
            if (++nacc >= 1) {            /* la boucle fait deja 200 ms */
                if (acc23 || acc45)
                    ESP_LOGW(TAG, "sur 200 ms : paire(2,3)=%+7ld   paire(4,5)=%+7ld",
                             (long)acc23, (long)acc45);
                acc23 = acc45 = 0; nacc = 0;
            }
        }

        if (tour % 25 == 0) {
            pu = !pu;
            gpio_set_pull_mode(PIN_ENC_A, pu ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
            gpio_set_pull_mode(PIN_ENC_B, pu ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
            vTaskDelay(pdMS_TO_TICKS(5));
            ESP_LOGW(TAG, "molette : tirage interne %s -> AB=%d%d",
                     pu ? "ACTIF " : "coupe ",
                     gpio_get_level(PIN_ENC_A), gpio_get_level(PIN_ENC_B));
        }

        if (++tour % 25 == 0)
            ESP_LOGW(TAG, "rebond : G %lu fronts/%lu ms (max %lu) | D %lu/%lu (max %lu)"
                          " | M %lu/%lu (max %lu)",
                (unsigned long)s_clics[0].fronts, (unsigned long)s_clics[0].rebonds,
                (unsigned long)s_clics[0].rebond_max,
                (unsigned long)s_clics[1].fronts, (unsigned long)s_clics[1].rebonds,
                (unsigned long)s_clics[1].rebond_max,
                (unsigned long)s_clics[2].fronts, (unsigned long)s_clics[2].rebonds,
                (unsigned long)s_clics[2].rebond_max);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
