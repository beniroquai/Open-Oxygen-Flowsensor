#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Jun  7 22:22:52 2021

@author: bene
"""
import csv
import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit

def func_1(x, a, b, c, d):
    return a+np.log(b+x*d)*c

def func(x, a, b, c, d):
    return a+np.sqrt(b+x*d)*c


dp_list = []    # differential pressure (Pa)
slm_list  = []  # flow (Standard Liters per Minute)

with open('FlowCalibrationData.csv', newline='') as csvfile:
    flowreader = csv.reader(csvfile, delimiter=';', quotechar='|')
    for row in flowreader:
        try:
            dp_list.append(np.float32(row[0]))
            slm_list.append(np.float32(row[1]))
        except:
            pass
        print(', '.join(row))
      
# let'S create a simple polynomial fit for the Arduino
z = np.polyfit(np.array(dp_list), np.array(slm_list), 12)
#z = np.polyfit(np.log(np.array(dp_list)), np.array(slm_list), 1)

popt, pcov = curve_fit(func, np.array(dp_list), np.array(slm_list))


p = np.poly1d(z)
slm_list_fit = p(np.array(dp_list))

#%%
# let's plot the result
plt.title('Flow Element - EK-P4 Evaluation Kit \n For SDP3x Differential Pressure Sensors')
plt.plot(dp_list,slm_list)
plt.plot(np.array(dp_list), func(np.array(dp_list), *popt))
plt.xlabel('differential pressure (Pa)')
plt.ylabel('flow (Standard Liters per Minute)')
plt.legend(['From datasheet', 'Fit'])
plt.savefig('Curve_Flow.png')
plt.show()

print("a,b,c,d")
print(*popt)

#%% now comes the oxygen sensor
signal_0percent_oxy = 0 # mV
sginal_209percent_oxy = 11 # mV

all_voltage_levels = np.linspace(0,100,100)
m = 20.9/(sginal_209percent_oxy-signal_0percent_oxy)
b = signal_0percent_oxy

all_oxygen_levels = (all_voltage_levels * m + b)

plt.title('Oxygen Sensor ')
plt.plot(all_voltage_levels,all_oxygen_levels)
plt.ylabel('Oxygen levels (%)')
plt.xlabel('Voltage levels (mV)')
plt.legend(['Calibrated using dry ice, mx+b, m='+str(np.round(m,3))+', b='+str(b)])
plt.savefig('Curve_Flow_Oxygen.png')