# Conchodytes v2 — relevés de banc

Notes prises au montage et au test de la **v1**, à reporter dans le layout de la
v2. Chaque point vient d'une observation sur carte réelle, pas d'une relecture
de schéma.

**Banc au 2026-08-25** — chaîne d'alimentation validée : batterie → protection
DW01A/FS8205 → Q2 (power-path) / D52 → **U20 HT7833 : 3,3 V mesuré et bon en
sortie**.

**Capteur validé le 2026-08-25**, après deux reworks au fer — le rail (`U100`
pins 2 et 3 croisées, voir `NOTE-LDO-PMW3360.md`) et la LED (§6) : identité lue,
SROM téléversé, déplacement mesuré, rail **mesuré à 2,01 V**. Le firmware de banc qui a servi
est dans `bringup/` — carte sur `/dev/ttyUSB2` via le FT2232 sur J1.

---

## 0. La puce n'est pas un PMW3360 — c'est un PMW3389

Lu sur la carte le 2026-08-25 : `Product_ID = 0x47`, `Inverse_Product_ID = 0xB8`,
`Revision_ID = 0x01`. Ce sont les valeurs par défaut du **PMW3389DM-T3QU**
(datasheet PixArt, Version 1.0 | 07 sep 2017, Table du §5.1, p. 20), pas celles
du PMW3360 (0x42 / 0xBD, p. 18 de sa propre datasheet). C'est **voulu** ; c'est
le dépôt qui n'avait pas suivi.

*(Coquille de la datasheet 3389 au passage : elle annonce `Inverse_Product_ID`
= 0xB9, alors que `0x47 ^ 0xFF = 0xB8`, et c'est bien 0xB8 que la carte renvoie.
Attendre le complément exact — c'est en prime une vérification de bus gratuite.)*

### Ce que les deux puces ne partagent pas

| | PMW3360 | PMW3389 |
|---|---|---|
| `Product_ID` | 0x42 | **0x47** |
| Résolution | `Config1` (0x0F), 8 bits | **`Resolution_L`/`_H` (0x0E/0x0F), 16 bits** |
| 0x0D | `Control` | **`Ripple Control`** |
| Config5 | 0x2F | **0x2E/0x2F** |
| LiftCutoff timeout | 0x58 / 0x5A | **0x71 / 0x72** |
| PWM | — | **0x73 / 0x74** |
| `tSRAD` (lecture normale) | 35 µs | **160 µs** |
| `tSWW` / `tSWR` | 120 µs | **180 µs** |
| Résistance de LED (référence) | 39 Ω sur 1,9 V | **13 Ω sur 1,9 V** |
| **Blob SROM** | — | **différent, 99,6 % des octets** |

Ce qui **ne** change **pas** : `VDD` 1,80/1,90/**2,10** V, `VDDIO` 1,80-3,60 V,
`fSCLK` ≤ 2 MHz, la lentille **LM19-LSI**, le brochage 16 broches, et la broche
15 `LED_P` en anode de la LED interne.

⚠️ Sur le 3389 les **2,10 V sont un maximum absolu** (Table 3, p. 15), pas
seulement le haut de la plage d'emploi comme sur le 3360.

### À reprendre dans le dépôt

- le symbole KiCad `U2` porte encore la valeur `PMW3360` ;
- `PMW3360.lib`, `PMW3360.pretty`, `NOTE-LDO-PMW3360.md` gardent leurs noms —
  le brochage 16 broches étant identique, l'empreinte reste valide, seul le nom
  ment ;
- le BOM et la commande LCSC.

---

## 1. U2 (PMW3389) — rotation de 180°

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

### ✅ Sens de rotation confirmé par la mesure (2026-08-25)

Protocole : passes dans **une seule direction**, souris soulevée pour le retour,
de sorte que le signe ne s'annule pas. Lecture par `Motion_Burst`, sommes signées
sur les échantillons amples.

| Geste | Convention HID | Mesuré | Biais |
|---|---|---|---|
| vers la **droite** | X positif | **X négatif** | 100,0 % sur 76 échantillons |
| vers l'**avant** | Y négatif (curseur monte) | **Y positif** | 99,8 % sur 71 échantillons |

**Les DEUX axes sont inversés.** C'est la signature exacte d'une rotation de
180° : une rotation quelconque mélangerait les axes (les deux paires varieraient
ensemble), un miroir n'en inverserait qu'un. Ici chaque axe reste propre — le
biais hors axe est de 50 % et d'amplitude 12 fois moindre — et les deux sont
retournés.

La correction proposée ci-dessus est donc la bonne, et le repli aussi :

- **rotation de 180° dans le layout** → les deux axes se remettent d'aplomb sans
  une ligne de firmware ;
- **si la lentille ne peut pas suivre** → le firmware nie `dx` et `dy`, deux
  signes moins, et le placement ne bouge pas.

Il ne reste donc plus qu'à vérifier **mécaniquement** si la LM19-LSI accepte la
rotation. La question logicielle, elle, est réglée.

**Question ouverte — la lentille.** Le PMW3389 travaille avec la lentille
**LM19-LSI** — la même que le 3360 (Figure 7, p. 10, et la note d'application
citée p. 13). Elle a ses propres ergots d'alignement sur le capteur et sur la
coque. Tourner le capteur de 180° peut imposer de tourner la lentille d'autant,
ce que la coque n'autorise peut-être pas. À trancher **avant** de figer le
layout : si la lentille ne peut pas suivre, la correction d'axe doit se faire
côté firmware plutôt que dans le placement.

*(Les deux datasheets sont maintenant indexées dans lemia : PMW3360DM-T2QU
`doc_id 6838`, PMW3389DM-T3QU `doc_id 6839`.)*

**Indice de banc à recouper avec ce point.** Le 2026-08-25, capteur à plat sur
l'établi **sans coque**, `SQUAL` fluctuait fortement au repos — de 12 à 73 sans
que rien ne bouge. Sur une optique correctement posée il devrait être stable. La
Table 4 (p. 15) veut **2,2 à 2,6 mm** entre le plan de référence de la lentille
et la surface suivie ; nue sur un établi la carte n'y est pas. À revérifier une
fois dans la M100 : si ça fluctue encore, c'est la lentille ou sa hauteur, pas le
capteur.

## 1bis. ⚠️ LQ1 EST CÂBLÉ À L'ENVERS — défaut de conception (BLOQUANT v2)

**Établi le 2026-08-26**, en confrontant le schéma d'origine de la M100
(`USB-Mouse-main/Mouse.sch`) au câblage de la Conchodytes.

`LQ1` n'est **pas** un double phototransistor A/COM/B. C'est un capteur à trois
fils **VCC / GND / DATA**. Marquage relevé sur le composant : **`H6Y07`**
(aucune datasheet trouvée).

### Le schéma d'origine, sans ambiguïté

```
(900, 6350) → label "Pin_4"    = LQ1 broche 1
(900, 6450) → label "Pin_2J"   = LQ1 broche 2
(900, 6550) → power:GNDREF     = LQ1 broche 3
```

`Pin_2J` n'est pas une masse : c'est le nœud d'alimentation, alimenté depuis
`Pin_2` à travers R12, et **partagé avec l'anode de LD1**.

### La comparaison

| `LQ1` | M100 d'origine | Conchodytes v1 | |
|---|---|---|---|
| broche 1 | `Pin_4` — **sortie DATA** | `ENC_A` + tirage 10 k | ✅ |
| broche 2 | `Pin_2J` — **VCC** | **la masse** | ❌ |
| broche 3 | **GNDREF** | `ENC_B` + tirage 10 k | ❌ |

**Le composant est alimenté à l'envers** : son VCC à la masse, sa masse tirée
vers le 3,3 V par R104.

### Ce que ça explique — toutes les mesures, sans exception

Le courant passe par la diode de substrat interne, de la broche GND du composant
vers sa broche VCC :

```
3,3 V → R104 (10 k) → broche GND du composant → jonction → broche VCC → masse
```

→ **0,65 V et 265 µA**, mesurés. Et par le même mécanisme sur la broche DATA
d'un circuit non alimenté.

| Observation | Expliquée |
|---|---|
| `ENC_A`/`ENC_B` à 0 en permanence | ✅ clampées à 0,65 V, sous le seuil VIL |
| Insensibles à la lumière (alu, scotch, R102 levée) | ✅ le composant n'est pas alimenté, la lumière n'y change rien |
| Impilotables même à 40 mA | ✅ une jonction en direct ne lâche pas |
| Invisibles à l'ohmmètre hors tension | ✅ le mode résistance travaille sous le seuil de conduction |
| **Identique sur DEUX composants différents** | ✅ c'est le comportement normal de ce câblage |

### Origine de l'erreur

Le symbole `Optical_Mouse:LQ`, hérité du fork `USB-Mouse`, ne nomme ses broches
que « 1, 2, 3 » **sans aucune fonction** :

```
X 1 1 -150    0 100 R 50 50 1 1 I
X 2 2 -150 -100 100 R 50 50 1 1 I
X 3 3 -150 -200 100 R 50 50 1 1 I
```

Le portage a donc supposé un A/COM/B classique. Rien dans le projet ne
contredisait cette supposition, et le schéma source n'a pas été consulté.

**À corriger en v2 :** donner des noms de broches au symbole (`DATA`, `VCC`,
`GND`) pour que l'erreur soit impossible à refaire, puis recâbler.

### ✅ Correction v2 — transposer le montage d'origine

> **APPLIQUÉE AU SCHÉMA le 2026-08-26.** Vérifiée à la netlist, ERC vert.
> **Le PCB n'est pas à jour** : ouvrir KiCad, « Mettre à jour le PCB depuis le
> schéma » (F8), puis reprendre le routage de ce coin.

**Principe retenu : reprendre le schéma de la M100 tel quel**, en transposant du
5 V USB au 3,3 V. C'est le seul montage dont on sait qu'il fonctionne avec ces
composants — et la récupération des pièces d'origine est le projet lui-même,
pas un pis-aller : un équivalent documenté et mécaniquement identique n'existe
probablement pas.

#### Le bloc encodeur de la M100

```
Pin_2 (VCC) ── JP1 (0 Ω) ──┬── Pin_2J ──┬── LQ1 broche 2  (VCC)
                           │            └── LD1 ANODE
                           └── C16 (non monté)

Pin_3 (Rot_LED) ── R14 (1500 Ω) ── LD1 CATHODE
Pin_4 (Rot_Enc) ─────────────────── LQ1 broche 1  (DATA)
GNDREF ──────────────────────────── LQ1 broche 3  (GND)
```

Deux traits à retenir : `LQ1` est **alimenté** (VCC/GND/DATA), et la **cathode
de LD1 est pilotée par le contrôleur**, pas câblée à la masse.

#### Le tableau des modifications

| Élément | v1 (faux) | **v2** |
|---|---|---|
| `LQ1.1` — DATA | `ENC_A` = GPIO7 | **inchangé** ✅ GPIO7 = `ADC1_CH6` |
| `LQ1.2` — VCC | **la masse** ❌ | **`+3V3`** |
| `LQ1.3` — GND | `ENC_B` = GPIO9 ❌ | **la masse** |
| `R103` 10 k sur `ENC_A` | +3,3 V → `ENC_A` | **garder** (inoffensive si la sortie est push-pull, nécessaire si collecteur ouvert) |
| `R104` 10 k sur `ENC_B` | +3,3 V → `ENC_B` | **supprimer** |
| `LD1` anode | `R102` → +3,3 V | **`+3V3` en direct** |
| `LD1` cathode | la masse | **`R102` → GPIO9**, renommé `ENC_LED` |
| `R102` | 330 Ω (netlist : 270) | **820 Ω** — voir calcul ci-dessous |

#### Pourquoi GPIO9 pour la LED

`ENC_B` disparaît en tant que signal, et `GPIO9` **arrive déjà physiquement dans
cette zone de la carte** — sa piste va aujourd'hui à `LQ1.3`. La réutiliser pour
commander la cathode de `LD1` évite de tirer une piste neuve à travers le
layout. Le routage change à peine.

À 2,6 mA, un GPIO de l'ESP32-S3 absorbe ce courant sans difficulté.

#### La valeur de R102

La M100 est en **5 V USB**, la Conchodytes en **3,3 V** : les valeurs ne se
recopient pas.

```
origine :  (5,0 − 1,16) / 1500 Ω  ≈  2,6 mA
v2      :  (3,3 − 1,16) / 820 Ω   ≈  2,6 mA
```

`Vf` = 1,16 V mesuré sous tension sur LD1. **820 Ω** reproduit donc exactement le
point de fonctionnement d'origine — nettement plus sobre que les 6,8 mA actuels,
ce qui est cohérent avec la saturation franche de `LQ1` observée au banc.

#### Ce que la LED pilotée apporte

1. **Autonomie.** La LED de molette peut être éteinte quand la souris dort. À
   2,6 mA en permanence, elle pèse lourd sur une batterie.
2. **Diagnostic.** Éteindre la LED par logiciel aurait réglé en une minute ce
   qui a pris une journée : plus besoin de lever `R102` au fer pour supprimer la
   source lumineuse.

#### Reste à caractériser : comment le sens de rotation sort d'un seul fil

La M100 défile dans les deux sens et `LQ1` n'a qu'une sortie — donc **cette
sortie porte l'information de sens**. Le mécanisme n'est pas établi.

Indice : ce contrôleur lit déjà **trois boutons sur un pont de résistances**
(`R13` = 1 MΩ, `R9`/`R10`/`R11` = 30 kΩ), donc par niveaux analogiques. Rien
n'interdit que `Rot_Enc` le soit aussi, `LQ1` combinant en interne ses deux
éléments en un niveau à plusieurs paliers.

*(Le `PAW3515DB`, même fabricant et même classe, utilise lui **deux** broches
`Z1`/`Z2` en quadrature classique — Figure 3, p. 3. Le `PAW3526D8` de la M100
est en boîtier 8 broches et n'en a pas les moyens : il doit caser VCC, GND, D+,
D−, trois clics, deux LED, le capteur et la molette sur huit pattes.)*

**L'expérience qui répond**, une fois la v2 câblée — ou dès maintenant avec deux
fils volants sur la v1 : alimenter `LQ1` correctement et **échantillonner
`ENC_A` à l'ADC** (`GPIO7` = `ADC1_CH6`) en tournant la roue.

- signal à **deux états** → un seul canal numérique, le sens vient d'ailleurs ;
- signal à **trois ou quatre paliers** → la quadrature est encodée en analogique,
  et un seul GPIO suffit à tout lire.

Le câblage v2 ci-dessus est bon dans les deux cas : il ne préjuge de rien et
respecte l'original.

### Note historique

Deux `LQ1` ont été dessoudés au cours du diagnostic, sur des conclusions
successives — « court franc à la masse », puis « composant mort » — toutes deux
fausses. **Les deux composants étaient sains.** Trois hypothèses ont été émises
et démenties avant que la confrontation au schéma d'origine ne donne la réponse.

La leçon : sur un composant repris d'un fork, **vérifier le schéma source avant
de faire confiance au symbole**, surtout quand celui-ci ne nomme pas ses
broches. Aucune mesure électrique ne pouvait révéler l'erreur — elles étaient
toutes cohérentes avec un câblage faux.

### Au passage : R102 vaut 330 Ω, pas 270

La netlist annonce 270 Ω, la valeur réellement montée sur la v1 est **330 Ω** —
à corriger dans le schéma pour qu'il cesse de mentir, **puis à remplacer par les
820 Ω du montage v2** (voir plus haut).

`LD1` est mesurée saine : `Vf` = 1,04 V en mode diode, **1,16 V sous tension**,
soit (3,3 − 1,16) / 330 ≈ **6,5 mA** aujourd'hui. La LED fonctionne — le
téléphone ne la voyait pas à cause de son filtre infrarouge, pas parce qu'elle
était éteinte.

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
| **R102** | résistance série de LD1 (LED de l'encodeur) — **330 Ω montée**, 270 Ω au schéma, **820 Ω en v2** (§1bis) | 0603 | (87,90 · 103,10) | **1,90 mm** |
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

**Résolu au banc le 2026-08-25** — c'était bien la seule cause de l'aveuglement
du capteur. Chiffres avant / après en fin de section.

Datasheet **PMW3389DM-T3QU** (PixArt, Version 1.0, 07 sep 2017) :

- **p. 4** : « *a power control circuit and **built-in LED driver integrated with
  IR LED** in a package* ».
- **p. 5**, Table 1 (brochage) : broche **15**, `LED_P`, type **Input**,
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

**Figure 10, p. 14** de la datasheet **3389** (*Reference Schematic diagram for
PMW3389DM-T3QU*), lue en la rendant en image :

```
  VDD_1.9V ── R2 = 13 R ── broche 15 (LED1_P)
```

L'alimentation de la LED vient du **rail capteur 1,9 V**, pas du 3,3 V, à travers
**13 Ω** — et non les 39 Ω du 3360. Le 3389 tire nettement plus de courant.

La même figure donne aussi `R1 = 10 kΩ` depuis le 3,3 V en tirage de `NRESET`,
ce que la carte fait déjà avec R100.

En reprenant la chute LED + driver interne de ≈ **1,43 V** déduite du schéma de
référence du 3360, on retrouve (1,9 − 1,43) / 13 ≈ **36 mA** — cohérent avec
l'`IDDRUN` de 21 mA moyen *LED comprise* du 3389 (p. 1 et p. 18), la LED étant
découpée en rapport cyclique. ⚠️ Cette chute de 1,43 V est **déduite**, jamais
lue dans une table du 3389 : toute valeur de résistance qui en sort doit être
vérifiée au voltmètre.

### Correction appliquée sur la v1 le 2026-08-25

Aucune piste coupée.

1. **D100 pontée.** C'est elle qui bloquait : routée cathode vers le +3,3 V, elle
   empêchait tout courant d'atteindre `LED_P`. Tant qu'elle est là, la valeur de
   R101 n'a aucune importance.
2. **R101 remplacée par 22 + 39 Ω en série = 61 Ω**, alimentée depuis le 3,3 V :
   (3,3 − 1,43) / 61 ≈ **30,7 mA**, soit 15 % sous les 36 mA de la référence.
   Volontairement du côté sobre — la LED est un gros poste sur une souris à
   batterie, et la mesure a montré qu'il reste énormément de marge optique.
3. Alimenter `LED_P` en 3,3 V est licite : **Table 3, p. 15** donne
   `VIN −0,5 … 3,6 V — All I/O pins`, et `LED_P` est typé *Input*.

⚠️ L'ancienne recommandation de cette note — **150 Ω depuis le 3,3 V** — visait
les 12 mA du **3360**. Sur le 3389 elle sous-alimente la LED d'un facteur 3.
Ne pas la réutiliser.

### Résultat mesuré

| | avant | après |
|---|---|---|
| `Shutter` | **4500** (plafond, obturateur grand ouvert) | **~50** |
| `SQUAL` | 3-5 | **48-81** |
| `dx` / `dy` | 0 en permanence | jusqu'à ±5000 par fenêtre de 200 ms |
| `MOTION` (GPIO18) | jamais actif | bascule à 0 sur mouvement |

Un obturateur qui passe de 4500 à 50, c'est un facteur 88 de lumière en plus.
Diagnostic confirmé : c'était D100, et rien d'autre.

### Correction pour la v2

> **APPLIQUÉE AU SCHÉMA le 2026-08-26.** `D100` supprimée, `R101` = 16 Ω
> alimentée depuis `+2V0`, rail renommé `+1V8` → `+2V0`. Netlist vérifiée :
> `+2V0` contient `R101.2`, et `Net-(U2-LED_P)` relie `R101.1` à `U2.15`.
> **PCB à mettre à jour.**

1. **Supprimer D100** du schéma, et avec lui la ligne 18 de
   `Niphargus/hardware/order-lcsc-actifs.csv` (« A CHOISIR … LED infrarouge
   940nm ») : il n'y a pas de référence à choisir, il n'y a pas de composant.
2. **R101 = 16 Ω, alimentée depuis le rail capteur** (`+2V0` après la correction
   du point U100), et non depuis le 3,3 V — comme Figure 10, p. 14.
   Le rail visé étant 2,0 V et non les 1,9 V du schéma de référence :
   (2,0 − 1,43) / 16 ≈ **35,6 mA**, soit le courant de la référence. Les 13 Ω de
   PixArt donneraient 43,8 mA sur 2,0 V, un peu haut.
   *(Le banc ayant montré une marge optique énorme à 30 mA, une valeur plus
   sobre — 22 Ω, soit 26 mA — est un arbitrage d'autonomie parfaitement
   défendable.)*
3. Penser à ce que le LDO U100 (XC6206P202, 120 mA) alimente **aussi** ce
   courant de LED, en plus du capteur — `IDDRUN` du 3389 est de 21 mA moyen LED
   comprise (p. 1), donc ça reste très large.

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

> **APPLIQUÉE AU SCHÉMA le 2026-08-26.** `VBAT_SENSE` est passée de `U6.21`
> (GPIO13, ADC2_CH2) à `U6.12` (**GPIO8, ADC1_CH7**). GPIO13 est libéré.
> **PCB à mettre à jour** : le pont R64/R67/C21 doit être rerouté de la
> rangée du bas vers la rangée de gauche du module, ~10 mm plus loin.

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
