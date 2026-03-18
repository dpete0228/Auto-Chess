tile_size_inches = 1.75
x_inches = 17.5
y_inches = 14
x_tiles = 10
y_tiles = 8

x_margin = 0.125
y_margin = 0

x_names = ["whitejail","h","g","f","e","d","c","b","a","blackjail"]
y_names = ["8","7","6","5","4","3","2","1"]

print("Position, X decimal, Y decimal")
print("Origin, 0.0, 0.0")

for c in range(x_tiles):

    if c is x_tiles-1:
        x = 1.0
    else:
        x = c * tile_size_inches
        if x != 0: x -= x_margin
        x /= (x_inches-tile_size_inches-(2*x_margin))

    for r in range(y_tiles):

        if r is y_tiles-1:
            y = 1.0
        else:
            y = r * tile_size_inches
            if y != 0: y -= y_margin
            y /= (y_inches-tile_size_inches-(2*y_margin))
        
        print(f"{x_names[c]}{y_names[r]}, {x}, {y}")