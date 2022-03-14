# TP1 d'info607

Simon LEONARD & Simon PICHENOT

## Question 3.4

|      Image       | Taille image |      Basic        | Avec compréssion de chemin | Avec union par rang | Avec les deux |
|:-----------------|:-------------|:------------------|:---------------------------|:--------------------|:--------------|
| pac-v1.png       | 8,8 K        |  522,569042    ms |        60,374960 ms        |       13,06486 ms   |   8,522293 ms |
| kowloon-1000.jpg | 367 K        |  seg fault        |       2786,64274 ms        |       58,41556 ms   |   35,42400 ms |
| lena.png         | 726 K        |  119295,516990 ms |       859,709304 ms        |       48,25563 ms   |   51,18408 ms |

Nous remarquons qu'en effet, l'optimisation ne sert pas à rien...
Cependant, nous n'avons pas compris pourquoi la seconde image prennait parfois plus de temps, voir produisait un segmentation fault.