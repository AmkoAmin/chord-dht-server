import math

m = int(input("Was ist die Bitlänge? "))
n = int(input("Was ist die Anzahl der Nodes? "))
k = int(input("Welche Node? "))

mod = 2 ** m   # int

for i in range(m):
    print(f"{i}: {(k + 2 ** i) % mod}")
