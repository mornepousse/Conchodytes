# ⚠️ U100 — le régulateur du capteur, à corriger avant de commander

Trouvé le 2026-08-09 en cherchant une référence LCSC pour ce régulateur. **Deux
défauts**, tous deux rattrapables tant que les gerbers n'ont pas été réuploadés.

## 1. Le rail n'est pas à la bonne tension

Le rail s'appelle `+1V8`. Or la datasheet PMW3360 (*Table 3, Recommended
Operating Conditions*, page 14) donne :

| Paramètre | Min | **Typ** | Max |
|---|---|---|---|
| VDD | **1,80** | **1,90** | 2,10 V |
| VDDIO | 1,80 | 1,90 | 3,60 V (doit être ≥ VDD) |

**1,80 V est le minimum absolu, pas la valeur nominale.** Un LDO 1,8 V avec sa
tolérance de ±2 % descend à 1,76 V, sous la spécification. Il faut viser 1,9 ou
2,0 V. VDDIO en 3,3 V reste correct.

Consommation à couvrir (*Table 5*, page 16) : **37 mA** en pointe de régime
(`IDD_RUN4`, LED comprise) et **70 mA de transitoire** pendant la rampe
d'alimentation. Un LDO de 100 mA minimum, 120 mA de préférence.

## 2. Le brochage de l'empreinte ne correspond à aucun composant

`U100` porte `lib_id = Regulator_Linear:MCP1700x-330xxTT`, mais ce symbole a son
brochage **modifié pour le HT7833 SOT-89** (1=GND, 2=VI, 3=VO) — c'est le
symbole trafiqué hérité du Niphargus, où il avait déjà causé un bloquant. Il a
été copié ici avec une empreinte **SOT-23**, boîtier où ce brochage n'existe pas.

Le PCB câble donc : pad 1 → GND, **pad 2 → +3.3V**, **pad 3 → +1V8**.

Brochage réel du XC6206 (datasheet Torex, *PIN ASSIGNMENT*, page 2) :

| | SOT-23 | SOT-89 |
|---|---|---|
| VSS | 1 | 1 |
| VIN | **3** | **2** |
| VOUT | **2** | **3** |

⚠️ **En SOT-23, les broches 2 et 3 sont inversées par rapport au câblage.** Monté
tel quel, le 3,3 V arrive sur la sortie du régulateur et le capteur reçoit 3,3 V,
soit très au-dessus de son maximum absolu de 2,10 V. **Destruction du PMW3360.**

Ne pas se fier aux sites de résumé de datasheets : trois d'entre eux ont donné
trois brochages différents pour ce composant, aucun correct. Seul le tableau du
fabricant fait foi.

## Correction retenue

**Garder le SOT-23 et échanger les pistes des pads 2 et 3 de `U100`**, puis
commander le **XC6206P202MR-G — LCSC `C2891260`** (2,0 V, SOT-23, 120 mA,
±2 % → 1,96-2,04 V, bien dans la plage 1,80-2,10).

L'alternative — passer l'empreinte en **SOT-89**, où le brochage 1=VSS 2=VIN
3=VOUT correspond déjà au câblage — a été écartée : le voisin le plus proche
(`R102`) est à 3,86 mm du centre de `U100`, et un SOT-89 fait 4,6 × 4,3 mm pads
compris. Ça passerait tout juste, sur une carte dont la mécanique est figée par
la coque M100.

Penser à renommer le rail `+1V8` en `+2V0` pour que le schéma ne mente pas.

## Point tranché : VDDPIX (résolu le 2026-08-25)

La broche 3 du capteur est décrite comme « **VDDPIX — Power — LDO output** »
(*Table 1*, page 3) : c'est une **sortie** de régulateur interne. Or le PCB la
relie au rail externe, en parallèle de VDD (pad 3 et pad 4 tous deux sur le même
net). La question était de savoir si PixArt le prescrit.

**Oui.** La figure a été lue en la rendant en image plutôt qu'en tentant d'en
extraire le texte. **Figure 10, page 12** (*Reference Schematics*, schéma
d'application filaire), bloc *Sensor Block* : les broches **3 (VDDPIX)** et
**4 (VDD)** sont toutes deux reliées au même rail `VDD6_SS`, tandis que la
broche **5 (VDDIO)** part sur `VDD4_3.3V`.

Le câblage du PCB est donc conforme au schéma d'application. **Rien à corriger
sur ce point.**

*(Au passage, la même figure a livré le montage de la LED — voir le point 6 de
`NOTES-V2.md`, où D100 se révèle être un composant de trop.)*
