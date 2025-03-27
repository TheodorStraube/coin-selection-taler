import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy.optimize import curve_fit

from collections import defaultdict

# Assuming files are stored in "saved-fees-updated" directory
data_dir = "../coin-selection-c-master/simulation/results"


def init_dataframe():
    files = os.listdir(data_dir)
    # Predefined colors for each strategy
    strategy_colors = defaultdict(lambda: "black")
    strategy_colors.update(
        {
            "MAX_BILLS": "blue",
   #         "CLOSEST_TO_EXPIRE_MIN_BILLS": "orange",
  #          "CLOSEST_TO_EXPIRE_MAX_BILLS": "cyan",
 #           "MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED": "yellow",
            "RANDOM": "magenta",
#            "MIN_BILLS": "green",
#            "EVEN_FROM_MIN_TO_MAX": "red",
#            "EVEN_FROM_MAX_TO_MIN": "pink",
            "GREEDY_MIN_TO_MAX": "purple",
            "GREEDY_MIN_TO_MAX_FIX": "red",
            "CALL_EXTERNAL": "cyan",
            "WALLET_CORE": "black"
        }
    )

    # Initialize an empty list to hold all data
    all_data = []

    # Process each file
    for file_name in files:
        file_path = os.path.join(data_dir, file_name)
        with open(file_path, "r") as file:
            # Read user type and strategy from the first line
            user_type, strategy = file.readline().strip().split(", ")

            # Parse the rest of the data
            for line in file:
                id, time, amount, operation, fee, coin_count = line.strip().split(
                    ", "
                )  # Updated here to include 'amount'
                all_data.append(
                    {
                        "UserType": user_type,
                        "Strategy": strategy,
                        "Id": id,
                        "Time": int(time),
                        "Amount": int(amount),
                        "Operation": operation,
                        "Fee": int(fee),
                        "FileName": file_name,
                        "CoinCount": int(coin_count)
                    }
                )

    # Convert the list of dictionaries to a DataFrame
    raw_df = pd.DataFrame(all_data)

    # Exclude DEPOSIT_REFRESH_OP so it wont be accounted for twice
    df = raw_df.copy().loc[
        (raw_df["Operation"] != "DEPOSIT_REFRESH_OP") & (raw_df["Amount"] > 0)
    ]
    df.describe()

    return strategy_colors, df, raw_df
