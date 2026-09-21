<h1 align="center">Conchodytes</h1>

<p align="center">
  <em>The shrimp that lives inside another's shell.</em><br>
  <em>La crevette qui vit dans la coquille d'un autre.</em>
</p>

---

## Firmware

The mouse runs the [KeSp firmware](https://github.com/mornepousse/KeSp_firmware)
(role `MOUSE`: PMW3389, three clicks, wheel, HID relayed to the dongle's slot 2).
Since 2026-09-21 **its board lives here**, not in the firmware repository:

- `boards/conchodytes/` — `board.h` (pinout, netlist-checked), `sdkconfig.defaults`,
  `test_pins.c` (the pinout contract);
- `firmware/` — the KeSp firmware as a git submodule, pinned on a release
  (`git submodule update --init`);
- `.github/workflows/firmware.yml` — calls the firmware's reusable workflow:
  every push builds `conchodytes_<version>.bin` (+ `_full.bin`), a `v*` tag
  publishes a release here.

```bash
scripts/test-pins.sh                                    # pinout contract, host, no toolchain
cd firmware && export IDF_COMPONENT_CHECK_NEW_VERSION=0 && \
idf.py -B build_conchodytes -DBOARD_DIR=../boards/conchodytes -DSDKCONFIG=build_conchodytes/sdkconfig build
esptool --chip esp32s3 -p /dev/ttyACM0 write_flash 0x20000 build_conchodytes/KeSp.bin
```

To take a newer firmware: `cd firmware && git fetch --tags && git checkout vX.Y.Z`,
then commit the submodule bump; read the firmware's release notes first (a
board macro may have been added — the pin contract will say).

## English

A **sleeper mouse**: a replacement PCB for a Logitech M100 shell, keeping the
outer appearance of a cheap office mouse while carrying a **PMW3389** sensor and
a **wireless link to the [KeSp dongle](https://github.com/mornepousse/KeSp_dongle)** — the same receiver as the
[Niphargus](https://github.com/mornepousse/Niphargus) keyboard.

The shell is a fixed constraint: click switches, optical wheel encoder and sensor
positions are imposed by the M100 case and are locked in the layout.

Electronics reuse the reviewed and routed blocks of Niphargus — ESP32-S3 +
nRF24L01+, TP4056 charging with DW01A/FS8205 protection, HT7833 LDO, USB-C with
ESD protection.

## Français

Une **souris sleeper** : un PCB de remplacement pour coque Logitech M100, qui
garde l'apparence d'une souris de bureau bon marché tout en embarquant un capteur
**PMW3389** et une **liaison sans fil vers le [dongle KeSp](https://github.com/mornepousse/KeSp_dongle)** — le même récepteur
que le clavier [Niphargus](https://github.com/mornepousse/Niphargus).

La coque est une contrainte figée : les positions des clics, de l'encodeur
optique de molette et du capteur sont imposées par le boîtier M100, et
verrouillées dans le layout.

L'électronique reprend les blocs relus et routés du Niphargus — ESP32-S3 +
nRF24L01+, charge TP4056 avec protection DW01A/FS8205, LDO HT7833, USB-C protégé.

**État (2026-08-25)** — capteur **validé au banc** : identifié, SROM téléversé,
déplacement lu, sur trois reworks (voir `NOTES-V2.md`). Le firmware de banc est
dans `bringup/`.

**État (2026-08-07)** — schéma complet et vérifié par netlist, board **routé,
0 pad non connecté**. Le contour et les positions imposées par la coque viennent
du fork `USB-Mouse` ; l'alimentation, la charge, l'USB et la radio sont repris
du Niphargus. Reste à nettoyer le DRC (18 `copper_edge_clearance`) et à mesurer
la coque pour figer le contour définitif.

---

## Repository layout · Organisation

| Path | Contents |
|---|---|
| `hardware/pcb/` | KiCad project — `conchodytes.kicad_pro` |
| `scripts/` | Anti-regression checks (same pipeline as Niphargus) |
| `docs/` | Design notes |

### Fabrication

The two projects stay **independent**. They are only merged **at gerber export
time**, so the mouse board is panelised into the free space of the Niphargus
panel and costs almost nothing to make.

Les deux projets restent **indépendants**. Ils ne sont réunis qu'**au moment de
l'export gerber**, la carte souris venant se loger dans le vide du panneau
Niphargus — le PCB revient alors à presque rien.

### Anti-regression · Anti-régression

```sh
./scripts/check.sh --fast   # ERC vs committed baseline
./scripts/check.sh          # + full DRC
```

### Bloc capteur

| | |
|---|---|
| **PMW3389** | SPI partagé avec le nRF24 (`SCK_L`/`MOSI_L`/`MISO_L`), plus `SNS_NCS` et `SNS_MOTION`. Mode SPI **3**, `fSCLK` ≤ 2 MHz |
| **Alimentation** | ⚠️ VDD et VDDPIX **ne sont pas en 3,3 V** — rail dédié **2,0 V** par LDO (1,80-2,10 V, maximum **absolu** 2,10). VDDIO reste en 3,3 V |
| **Optique** | lentille **LM19-LSI**, commune au PMW3360 et au PMW3389 — celle d'origine de la M100 est faite pour le PAW3526 et ne convient pas |
| **Molette** | encodeur **optique** : LD1 éclaire une roue à 60 fentes, LQ1 la lit. ⚠️ `LQ1` (marquage `H6Y07`) est un capteur **VCC / GND / DATA**, PAS un double phototransistor — la v1 le câblait à l'envers, voir `NOTES-V2.md` §1bis |
| **Clics** | SPDT, COM à la masse, **NO et NC** tirés chacun au 3,3 V — lire les deux contacts en parallèle supprime le rebond |

Affectation des GPIO du S3, en évitant les broches de strapping :

| Signal | GPIO | Broche du module | | Signal | GPIO | Broche |
|---|---|---|---|---|---|---|
| `SNS_NCS` | 17 | 10 | | `SW_LEFT` | 10 | 18 |
| `SNS_MOTION` | 18 | 11 | | `SW_RIGHT` | 11 | 19 |
| `ENC_A` (DATA) | 7 | 7 | | `SW_MID` | 12 | 20 |
| `ENC_LED` ¹ | 9 | 17 | | `SW_LEFT_NC` | 5 | 5 |
| | | | | `SW_RIGHT_NC` | 4 | 4 |
| | | | | `SW_MID_NC` | 6 | 6 |

¹ `GPIO9` portait `ENC_B` en v1, sur un montage faux. En v2 il **commande la
cathode de LD1** — la LED de molette peut donc être éteinte par le firmware,
ce qui compte sur une souris à batterie. `ENC_A` (GPIO7) est le seul signal de
l'encodeur, et c'est `ADC1_CH6`.

⚠️ Ne pas confondre **GPIO** et **numéro de broche du module** : ce tableau
donnait auparavant les broches en les présentant comme des GPIO.

⚠️ **`SW_LEFT_NC` et `SW_RIGHT_NC` étaient inversés ici** jusqu'au 2026-08-25.
Le tableau annonçait GPIO4 à gauche et GPIO5 à droite ; la netlist dit l'inverse
— GPIO4 va à `SW2.3` (clic droit, R109) et GPIO5 à `SW1.3` (clic gauche, R108).
Comme l'anti-rebond apparie NO et NC du **même** bouton, écrire le firmware sur
l'ancien tableau croisait les deux clics.

Le reste du brochage, relevé à la netlist au `kicad-cli` le 2026-08-25 :

| Signal | Broche | GPIO | | Signal | Broche | GPIO |
|---|---|---|---|---|---|---|
| `SCK_L` | 31 | 38 | | `CSN_1` (nRF) | 38 | 2 |
| `MISO_L` | 32 | 39 | | `CE_1` (nRF) | 39 | 1 |
| `MOSI_L` | 33 | 40 | | `IRQ_1` (nRF) | 34 | 41 |
| `VBAT_SENSE` ² | 21 | 13 (ADC2_CH2) | | `D-` / `D+` | 13 / 14 | 19 / 20 |
| J1 `TX` / `RX` | 37 / 36 | 43 / 44 | | J1 `IO0` | 27 | 0 |

² En **v2**, `VBAT_SENSE` passe sur la broche 12 = **GPIO8 = ADC1_CH7**. L'ADC2
de l'ESP32-S3 est partagé avec le driver Wi-Fi ; tant que la mesure y reste, le
Wi-Fi est interdit sur cette carte. Voir `NOTES-V2.md` point 7.

Les lignes SPI et de contrôle portent chacune 100 Ω en série (R16/R18/R30,
R36/R39/R41) — sans effet à 2 MHz.

Les contacts NC des trois clics sont câblés sur GPIO4/5/6, chacun avec son
pull-up 10 k (R108-R110). Au repos NC est fermé sur COM donc bas, NO ouvert donc
haut ; au clic ils s'inversent. **Pendant le rebond les deux sont hauts** — le
firmware garde alors l'état précédent, ce qui élimine le double-clic sans aucun
filtrage temporel.

Les six empreintes imposées par la coque — SW1/SW2/SW3, LD1, LQ1, U2 — sont
**verrouillées** dans le layout et liées à leurs symboles. Ne pas les renommer :
le F8 les dissocierait et leurs positions seraient perdues.

## Historique · History

| Étape | Ce qui s'est passé |
|---|---|
| **REV1** | Premier tirage. Trois défauts de conception trouvés au banc, tous documentés dans `NOTES-V2.md` : `LQ1` câblé à l'envers (§1bis), `U100` — le LDO du capteur — pattes 2 et 3 croisées, et `U2` monté à 180°. |
| **2026-08-25** | Bring-up du capteur. La puce n'est **pas** un PMW3360 mais un **PMW3389** (`Product_ID = 0x47`) : identité lue, SROM téléversé, déplacement mesuré. Rail capteur corrigé au fer, **2,01 V mesuré**. Rotation de 180° confirmée par la mesure — les deux axes inversés, biais 100 % sur 76 échantillons. |
| **REV2.0.0** | Schéma corrigé et gerbers partis en fabrication : `LQ1` recâblé sur `+3V3`, `D100` supprimée, `VBAT_SENSE` déplacé sur ADC1, `U100` corrigé, `LD1` passée en cathode pilotée. |
| **2026-08-26** | Firmware complet et lien radio. La souris parle au dongle KaSe sur le slot RF 2 — le code vit comme septième variante de carte dans `KeSp_firmware`, pas dans un dépôt séparé. |

### Ce que la mise au point du 26 août a trouvé

Quatre défauts, chacun établi par une mesure prise **aux deux extrémités du
lien** — compteurs de la souris d'un côté, `RF_STATUS` du dongle de l'autre.

| Défaut | Avant | Après |
|---|---|---|
| Bascule de mode SPI sous un CS déjà abaissé — bus partagé entre le capteur (mode 3) et la radio (mode 0) | 1 trame acquittée sur 738 | 693 sur 693 |
| Scrutation du dongle à 10 ms, pour une FIFO nRF24 de 3 paquets | 3135 acceptées sur 8356 (37,5 %) | 99-100 % |
| Déplacement des trames non acquittées **jeté** au lieu d'être réaccumulé | la moitié du geste disparaissait | distance conservée |
| Résolution jamais écrite : la puce restait à son défaut | 5000 cpi | 1000 cpi, comme la M100 |

### Ce qui reste ouvert

Un tremblement d'**origine optique** que le firmware ne peut pas corriger :
`SQUAL` mesuré entre 30 et 55 là où ~80 est attendu, et le capteur produit sur
certaines surfaces des dérives cohérentes de ~1000 comptes en trois secondes,
souris immobile — indiscernables d'un geste lent volontaire.

Deux pistes, par ordre de coût croissant, détaillées au **§8 de `NOTES-V2.md`** :

1. **`R102`** vaut 330 Ω sous 3,3 V, soit ~6,8 mA dans `LD1`, quand la M100
   d'origine en fait passer 2,6 mA. La LED de molette tourne à près du triple de
   son intensité prévue, en permanence, contre le capteur. **820 Ω** reproduit
   le point de fonctionnement d'origine. Un composant.
2. **Le montage à 180° de `U2`** échange peut-être les chemins d'illumination et
   d'imagerie de la lentille, ce qui expliquerait l'image à la fois sombre en
   moyenne et saturée par endroits. Si c'est le cas, la rotation d'empreinte
   prévue en REV2 corrige le tremblement **et** le sens des axes d'un coup.

## Credits

Outline and mechanical positions derive from
[RandomDelta6/USB-Mouse](https://github.com/RandomDelta6/USB-Mouse), a working
USB mouse PCB reusing the original optical assembly.
