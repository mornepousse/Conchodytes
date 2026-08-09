<h1 align="center">Conchodytes</h1>

<p align="center">
  <em>The shrimp that lives inside another's shell.</em><br>
  <em>La crevette qui vit dans la coquille d'un autre.</em>
</p>

---

## English

A **sleeper mouse**: a replacement PCB for a Logitech M100 shell, keeping the
outer appearance of a cheap office mouse while carrying a **PMW3360** sensor and
a **wireless link to the KaSe dongle** — the same receiver as the
[Niphargus](https://github.com/mornepousse/Niphargus) keyboard.

The shell is a fixed constraint: click switches, optical wheel encoder and sensor
positions are imposed by the M100 case and are locked in the layout.

Electronics reuse the reviewed and routed blocks of Niphargus — ESP32-S3 +
nRF24L01+, TP4056 charging with DW01A/FS8205 protection, HT7833 LDO, USB-C with
ESD protection.

## Français

Une **souris sleeper** : un PCB de remplacement pour coque Logitech M100, qui
garde l'apparence d'une souris de bureau bon marché tout en embarquant un capteur
**PMW3360** et une **liaison sans fil vers le dongle KaSe** — le même récepteur
que le clavier [Niphargus](https://github.com/mornepousse/Niphargus).

La coque est une contrainte figée : les positions des clics, de l'encodeur
optique de molette et du capteur sont imposées par le boîtier M100, et
verrouillées dans le layout.

L'électronique reprend les blocs relus et routés du Niphargus — ESP32-S3 +
nRF24L01+, charge TP4056 avec protection DW01A/FS8205, LDO HT7833, USB-C protégé.

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
| **PMW3360** | SPI partagé avec le nRF24 (`SCK_L`/`MOSI_L`/`MISO_L`), plus `SNS_NCS` et `SNS_MOTION` |
| **Alimentation** | ⚠️ VDD et VDDPIX **ne sont pas en 3,3 V** — rail dédié 1,8 V par LDO. VDDIO reste en 3,3 V |
| **Optique** | lentille **LM19-LSI**, propre au PMW3360 — celle d'origine de la M100 est faite pour le PAW3526 et ne convient pas |
| **Molette** | encodeur **optique** : LD1 éclaire une roue à 60 fentes, LQ1 double phototransistor → `ENC_A`/`ENC_B` |
| **Clics** | SPDT, COM à la masse, **NO et NC** tirés chacun au 3,3 V — lire les deux contacts en parallèle supprime le rebond |

Affectation des GPIO du S3, en évitant les broches de strapping :

| Signal | GPIO | Broche du module | | Signal | GPIO | Broche |
|---|---|---|---|---|---|---|
| `SNS_NCS` | 17 | 10 | | `SW_LEFT` | 10 | 18 |
| `SNS_MOTION` | 18 | 11 | | `SW_RIGHT` | 11 | 19 |
| `ENC_A` | 7 | 7 | | `SW_MID` | 12 | 20 |
| `ENC_B` | 9 | 17 | | `SW_LEFT_NC` | 4 | 4 |
| | | | | `SW_RIGHT_NC` | 5 | 5 |
| | | | | `SW_MID_NC` | 6 | 6 |

⚠️ Ne pas confondre **GPIO** et **numéro de broche du module** : ce tableau
donnait auparavant les broches en les présentant comme des GPIO.

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
