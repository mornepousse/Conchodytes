<h1 align="center">Conchodytes</h1>

<p align="center">
  <em>The shrimp that lives inside another's shell.</em><br>
  <em>La crevette qui vit dans la coquille d'un autre.</em>
</p>

---

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

## Credits

Outline and mechanical positions derive from
[RandomDelta6/USB-Mouse](https://github.com/RandomDelta6/USB-Mouse), a working
USB mouse PCB reusing the original optical assembly.
