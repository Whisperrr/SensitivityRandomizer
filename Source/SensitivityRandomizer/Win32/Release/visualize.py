import matplotlib.pyplot as plt

def plot():
    '''Plot output from sensitivity randomizer.'''

    x_vals = []
    y_vals = []

    try:
        with open('sens_list.txt') as fp:
            lims = [float(e) for e in fp.readline().strip().split(",")]

            for line in fp:
                vals = [float(e) for e in line.strip().split(",")]
                #print(vals)
                x_vals.append(vals[0])
                y_vals.append(vals[1])

        if lims[3] == 0:
            new_x = [x_vals[0]]
            new_y = [y_vals[0]]
            for i in range(1, len(x_vals)):
                new_x.append(x_vals[i]-0.00001)
                new_x.append(x_vals[i])
                new_y.append(y_vals[i-1])
                new_y.append(y_vals[i])

            x_vals = new_x
            y_vals = new_y

        plt.title("Sensitivity Randomizer")
        plt.xlabel("Time (seconds)")
        plt.ylabel("Sensitivity Multiplier")
        plt.ylim(lims[1]-0.1, lims[2]+0.1)
        plt.plot(x_vals, y_vals)
        plt.show()

    except FileNotFoundError:
        print("Sensitivity list not found. Please first run SensitivityRandomizer.exe to generate output.")

if __name__ == "__main__":
    plot()
