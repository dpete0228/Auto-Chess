tile_size_inches = 1.75
x_inches = 17.5
y_inches = 14
x_tiles = 10
y_tiles = 8

x_origin_loss = 1/8
y_origin_loss = 0

columns = ["whitejail","h","g","f","e","d","c","b","a","blackjail"]
rows = ["8","7","6","5","4","3","2","1"]

print("Position, X percent, Y percent")
print("Origin, 0, 0")

for c in range(10):

    for r in range(8):
        if r is 7:
            y = 1.0