import matplotlib.pyplot as plt

def plot():
    '''Plot output from sensitivity randomizer.'''

    x_vals = []
    y_vals = []

    try:
        with open('sens_list.txt') as fp:
            lims = [float(e) for e in fp.readline().strip().split(",")]
            print(lims)

            for line in fp:
                vals = [float(e) for e in line.strip().split(",")]
                #print(vals)
                x_vals.append(vals[0])
                y_vals.append(vals[1])

        plt.title("Smooth Sensitivity Randomization")
        plt.xlabel("Time (seconds)")
        plt.ylabel("Sensitivity Multiplier")
        plt.ylim(lims[1], lims[2])
        plt.plot(x_vals, y_vals)
        plt.show()
    except FileNotFoundError:
        print("Sensitivity list not found. Please first run SensitivityRandomizer.exe to generate output.")

if __name__ == "__main__":
    plot()
