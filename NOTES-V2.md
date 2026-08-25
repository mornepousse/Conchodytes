# Conchodytes v2 — relevés de banc

Notes prises au montage et au test de la **v1**, à reporter dans le layout de la
v2. Chaque point vient d'une observation sur carte réelle, pas d'une relecture
de schéma.

**Banc au 2026-08-25** — chaîne d'alimentation validée : batterie → protection
DW01A/FS8205 → Q2 (power-path) / D52 → **U20 HT7833 : 3,3 V mesuré et bon en
sortie**. L'étage capteur n'a pas encore été mis sous tension (voir
`NOTE-LDO-PMW3360.md`, toujours ouvert et bloquant).

---

## 1. U2 (PMW3360) — rotation de 180°

Le capteur est du bon côté de la carte, mais **orienté à l'envers** : l'axe Y est
inversé par rapport à l'axe de la souris.

| | v1 | v2 |
|---|---|---|
| Face | F.Cu | F.Cu (inchangé) |
| Position | (96,53 / 116,38) | inchangée |
| Rotation | 0° | **180°** |

La position ne bouge pas — elle est imposée par la coque M100 et l'empreinte est
verrouillée dans le layout. Seule la rotation change.

⚠️ **Ne pas renommer U2** en faisant la modification : le README rappelle que les
six empreintes imposées par la coque (SW1/SW2/SW3, LD1, LQ1, U2) sont liées à
leurs symboles, et qu'un F8 les dissocierait — leurs positions seraient perdues.

**Question ouverte — la lentille.** Le PMW3360 travaille avec la lentille
**LM19-LSI**, qui a ses propres ergots d'alignement sur le capteur et sur la
coque. Tourner le capteur de 180° peut imposer de tourner la lentille d'autant,
ce que la coque n'autorise peut-être pas. À vérifier sur la datasheet PixArt
**avant** de figer le layout : si la lentille ne peut pas suivre, la correction
d'axe doit se faire côté firmware plutôt que dans le placement.

*(La datasheet PMW3360 n'est pas indexée dans la bibliothèque lemia — l'y ajouter
avec `add_document` avant de trancher.)*

## 2. Trou de maintien à gauche de la molette — à repositionner et à élargir

Découpe circulaire du contour, **Ø 1,00 mm, centre (85,85 / 93,36)**, juste sous
LD1 (la LED de l'encodeur), côté clic gauche.

Deux défauts constatés au montage :
- il est **trop petit** ;
- il n'est **pas à la bonne place**.

**Bloqué en attente de mesures sur la coque M100.** Ne rien corriger au jugé : le
diamètre et la position définitifs sortiront du relevé mécanique. Les deux autres
découpes du même secteur — Ø 1,50 mm à (105,47 / 94,09) et Ø 4,00 mm à
(112,91 / 93,29) — ne sont pas concernées par ce point, mais autant les vérifier
pendant qu'on a la coque et le pied à coulisse en main.

## 3. Clics en hotswap

Passer SW1 / SW2 / SW3 sur des **supports hotswap** au lieu du soudé direct, pour
pouvoir changer les switches sans fer.

État v1 : empreinte `Mouse:SPDT_Switch` — trois pastilles **traversantes en
ligne, pas de 5,08 mm, perçage 1,2 mm** (le brochage classique des switches de
souris type Omron D2FC).

Points à trancher avant de router :
- **Référence du support** à choisir (les sockets hotswap pour switch de souris
  existent, mais le brochage exact et l'empreinte dépendent du modèle). Vérifier
  la disponibilité LCSC avant de dessiner l'empreinte.
- **Face de montage** : ces supports se soudent en général sur la face *opposée*
  au switch. SW1/SW2/SW3 étant en F.Cu, les supports iraient en B.Cu — il faut
  donc de la garde verticale sous la carte à cet endroit.
- **La coque est une contrainte figée.** La hauteur ajoutée par le support
  déplace le switch de l'épaisseur du socket ; si la course du clic est déjà
  juste dans la M100, ça peut suffire à casser le ressenti, voire à empêcher la
  coque de fermer. À mesurer avant de s'engager.
- Le montage NO + NC anti-rebond (COM à la masse, les deux contacts tirés au
  3,3 V, GPIO 4/5/6 et 10/11/12) doit être conservé : le support doit donc bien
  reprendre **les trois broches**, pas seulement COM et NO.

## 4. Découpe interne de la molette — revoir le côté gauche

La fente de passage de la molette est un contour interne allant de
**x 87,32 à 106,35** et **y ≈ 84 à 99,98**. Son bord gauche n'est pas droit : il
porte un **décrochement**.

```
        x=87,32   x=90,27
           |         |
  y=86,46  |     +---+           <- le bord gauche commence a x=90,27
           |     |
  y=92,45  +-----+               <- puis saute a x=87,32 : le decrochement
           |
           |     ( molette )
           |
  y=99,98  +---------------------+  x=106,35   <- bord bas
```

Segments concernés, à revoir ensemble :
`(87,315 · 86,458)→(90,269 · 86,458)`, `(90,269 · 86,458)→(90,269 · 92,456)`,
`(90,269 · 92,456)→(87,315 · 92,448)`, `(87,315 · 92,448)→(87,315 · 99,978)`.

Au-dessus de y = 92,45 la fente est **2,95 mm plus étroite** qu'en dessous. À
confirmer sur la coque : soit le décrochement est justifié par un ergot de la
M100 et il faut le garder tel quel, soit c'est un reliquat du fork `USB-Mouse` et
la fente doit être droite sur toute sa hauteur.

**Même zone que le point 2** (le trou de maintien Ø 1,00 mm à gauche) et que
R102 au point 5 : tout le coin gauche de la molette est à reprendre d'un bloc,
à partir du même relevé mécanique. Ne pas traiter ces trois points séparément.

## 5. Éloigner D52 et R102 de la molette

Les deux composants sont en **F.Cu, soit la face de la molette**, et posés juste
sous le bord bas de sa découpe (y = 99,978) :

| Réf | Rôle | Empreinte | Centre | Garde au bord de la fente |
|---|---|---|---|---|
| **R102** | 270 Ω, résistance série de LD1 (LED de l'encodeur) | 0603 | (87,90 · 103,10) | **1,90 mm** |
| **D52** | SS14, diode d'alimentation (USB +5 V → VIN de U20) | **D_SMA** | (104,90 · 103,10) | **2,22 mm** |

Les éloigner par sécurité. Deux raisons distinctes :

- **D52** est en boîtier **SMA** — nettement plus haut qu'un 0603 — et se trouve
  sous la partie droite de l'ouverture. C'est le plus exposé au frottement de la
  roue ou de son axe.
- **R102** est à 1,90 mm seulement, et se trouve **dans l'emprise en x de la
  fente** (87,43-88,38, quand la fente commence à 87,32) : elle est franchement
  sous l'ouverture, dans le coin gauche déjà signalé aux points 2 et 4.

La garde visée dépend de l'encombrement réel de la roue et de son support —
**à sortir du même relevé mécanique** que les points 2 et 4, pas à choisir au
jugé. R102 doit rester courte vers LD1 (83,97 · 89,46), mais un 0603 de LED n'a
aucune contrainte de routage : elle peut descendre ou partir vers la gauche
sans conséquence électrique.

## 6. D100 est de trop, et R101 a la mauvaise valeur

Datasheet **PMW3360DM-T2QU** (PixArt, Version 1.30, 6 avril 2016) :

- **p. 1**, description générale : « *DIP package with **integrated IR LED*** ».
- **p. 3**, Table 1 (brochage) : broche **15**, `LED_P`, type **Input**,
  description « **LED Anode** ».

Le capteur embarque sa LED infrarouge. La broche 15 est l'anode de cette LED
interne, à alimenter depuis un rail via une résistance de limitation. **Il n'y a
aucune LED externe à monter** — D100 (« IR 940nm », 0805) n'a pas lieu d'être.

Pire, telle que la carte est routée :

```
  +3.3V ── D100 pad1 (CATHODE) ── pad2 (ANODE) ── R101 100R ── LED_P
```

La LED est traversée à l'envers : elle **bloque** le courant. La LED interne du
capteur ne s'allume pas et le capteur est aveugle.

### Montage de référence

**Figure 10, p. 12** (schéma d'application filaire) — bloc *Sensor Block* :

```
  VDD5_SS (rail 1,9 V) ── R9 = 39 R ── broche 15 (LED_P)
```

L'alimentation de la LED vient du **rail capteur 1,9 V**, pas du 3,3 V, à travers
**39 Ω**. Avec les 12 mA nominaux de la Table 5 (p. 16), la chute LED + driver
interne vaut 1,9 − 0,012 × 39 ≈ **1,43 V** — c'est ce chiffre qui permet de
recalculer la résistance depuis n'importe quel rail.

### Correction sur la v1 (carte déjà fabriquée)

Aucune piste à couper.

1. **Ne pas monter D100.** Mettre un **pont** à sa place : une 0 Ω 0805, ou une
   goutte de soudure entre les deux pastilles.
2. **Remplacer R101 par 150 Ω** (au lieu de 100 Ω). Depuis 3,3 V :
   (3,3 − 1,43) / 150 ≈ **12,5 mA**, contre ≈ 19 mA avec les 100 Ω d'origine.
3. Alimenter LED_P en 3,3 V est licite : **Table 2, p. 14** donne
   `VIN −0,5 … 3,6 V — All I/O pins`, et LED_P est typé *Input*.
4. **Vérification au banc** : mesurer la tension aux bornes de R101, le courant
   vaut V / 150. Viser les 12 mA de la Table 5. La chute de 1,43 V est déduite du
   schéma de référence, pas lue dans une table — la valeur réelle peut s'en
   écarter, d'où la mesure.

### Correction pour la v2

1. **Supprimer D100** du schéma, et avec lui la ligne 18 de
   `Niphargus/hardware/order-lcsc-actifs.csv` (« A CHOISIR … LED infrarouge
   940nm ») : il n'y a pas de référence à choisir, il n'y a pas de composant.
2. **R101 = 47 Ω, alimentée depuis le rail capteur** (`+2V0` après la correction
   du point U100), et non depuis le 3,3 V — comme Figure 10.
   Le rail visé étant 2,0 V et non les 1,9 V du schéma de référence :
   (2,0 − 1,43) / 47 ≈ **12,1 mA**. Les 39 Ω de PixArt donneraient 14,6 mA sur
   2,0 V, soit un peu haut.
3. Penser à ce que le LDO U100 (XC6206P202, 120 mA) alimente **aussi** ces
   ~12 mA de LED, en plus des 37 mA du capteur — ça reste très large.

## 7. VBAT_SENSE : passer la mesure batterie sur ADC1

Le pont diviseur lui-même **ne change pas**. C'est la broche qui le reçoit qui
est mal choisie.

### Le pont est bon, on n'y touche pas

```
+BATT ──[ R64 1M ]──┬── VBAT_SENSE
                    │
                   [ R67 1M ]   [ C21 100nF ]
                    │            │
                   GND          GND
```

Rapport ÷2 : 4,2 V batterie pleine → 2,1 V à l'entrée, dans la plage de l'ADC.
Les 1 MΩ ne sont pas une étourderie mais un choix de consommation — **2,1 µA**
de fuite permanente, contre 21 µA avec un 100 k/100 k. Pour un objet qui passe
sa vie endormi, c'est le bon arbitrage, et **C21 (100 nF) est déjà présent** :
le condensateur d'échantillonnage de l'ADC puise dedans et non à travers le
1 MΩ, ce qui est exactement ce qui rend un pont haute impédance utilisable.

Reste l'erreur due au courant de fuite de la broche ADC sur 500 kΩ d'impédance
de Thévenin. Elle se rattrape par une **calibration deux points en firmware**
(batterie pleine / batterie basse contre un voltmètre) — de toute façon
nécessaire sur un ADC ESP32, pont basse impédance ou pas.

### Ce qui change : la broche

Aujourd'hui `VBAT_SENSE` arrive sur la **broche 21** du module. Datasheet
**ESP32-S3-WROOM-1/1U, Table 3-1 *Pin Definitions*, p. 11** :

```
IO13    21    I/O/T    RTC_GPIO13, GPIO13, TOUCH13, ADC2_CH2, FSPIQ, FSPIIO7, SUBSPIQ
```

C'est donc **ADC2_CH2**. Or sur ESP32-S3 l'ADC2 est partagé avec le driver
Wi-Fi : tant que la mesure batterie est là, **le Wi-Fi est interdit sur cette
carte**. Ça marche aujourd'hui — la radio est un nRF24 et le firmware n'allume
jamais le Wi-Fi — mais c'est une hypothèque posée sur toutes les évolutions
futures, pour un gain nul.

**Décision : déplacer la mesure sur ADC1 en v2**, plutôt que d'inscrire
l'interdiction du Wi-Fi dans le marbre.

| | Broche module | GPIO | Canal | |
|---|---|---|---|---|
| Aujourd'hui | 21 | GPIO13 | ADC2_CH2 | à quitter |
| **Cible** | **12** | **GPIO8** | **ADC1_CH7** | libre, hors broches de strapping |

`GPIO8` est aujourd'hui non connecté sur la carte, et n'est pas une broche de
strapping. Ses autres fonctions (TOUCH8, SUBSPICS1) ne sont pas utilisées.

**Alternative écartée** : broche 15 = `GPIO3` = ADC1_CH2, libre elle aussi, mais
la même page 11 la liste comme **broche de strapping** — à éviter pour une
entrée analogique tirée en permanence par un pont diviseur.

Conséquence layout : le pont R64/R67/C21 doit être rerouté de la broche 21 vers
la broche 12. Ce n'est pas un déplacement voisin — dans l'empreinte du module,
la broche 21 est sur la **rangée du bas** (offset local 0,635 / 9,50) et la
broche 12 sur la **rangée de gauche** (−8,75 / 5,71), soit ~10 mm d'écart et un
changement de côté.
