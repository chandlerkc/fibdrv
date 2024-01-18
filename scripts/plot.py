#!/usr/bin/env python3

import matplotlib.pyplot as plt
import csv

X1 = []
Y1 = []
X2 = []
Y2 = []

with open('out_double_fasting', 'r') as datafile:
    plotting = csv.reader(datafile, delimiter=' ')
    for ROWS in plotting:
        X1.append(int(ROWS[0]))
        Y1.append(int(ROWS[1]))
plt.scatter(X1, Y1, color='blue')

with open('out_iterate', 'r') as datafile:
    plotting = csv.reader(datafile, delimiter=' ')
    for ROWS in plotting:
        X2.append(int(ROWS[0]))
        Y2.append(int(ROWS[1]))
plt.scatter(X2, Y2, color='red')
plt.title('Line Graph using CSV')
plt.xlabel('X')
plt.ylabel('Y')
plt.show()
