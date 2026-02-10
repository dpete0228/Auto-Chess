print("Position, X inches, Y inches")
print("Origin, 0, 0")

colors = ["Black","White"]
dec = 0.875
for color in colors:
    y = 13.125
    for i in range(15):
        print(f"{color}Jail{i+1},{" " if i+1<10 else ""} -0.875, {" " if y<10 else ""}{y:.3f}")
        y -= dec

letters = ["a","b","c","d","e","f","g","h"]
inc = 1.75
x = -0.875
for i in range(8):
    x += inc
    y = -0.875
    for j in range(8):
        y += inc
        print(f"{letters[i]}{j+1}, {" " if x<10 else ""}{x}, {" " if y<10 else ""}{y}")