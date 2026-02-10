print("Position, X inches, Y inches")
print("Origin, 0, 0")

colors = [("Black", "-0.875"),("White", "14.875")]
dec = 0.875
for pair in colors:
    y = 13.125
    for i in range(15):
        print(f"{pair[0]}Jail{i+1},{" " if i+1<10 else ""} {pair[1]}, {" " if y<10 else ""}{y:.3f}")
        y -= dec

letters = ["a","b","c","d","e","f","g","h"]
inc = 1.75
x = 0.875
for i in range(8):
    y = 0.875
    for j in range(8):
        print(f"{letters[i]}{j+1}, {" " if x<10 else ""}{x}, {" " if y<10 else ""}{y}")
        y += inc
    x += inc
