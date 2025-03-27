import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy.optimize import curve_fit


def balance(df: pd.DataFrame, strategy_colors: dict):

    fig, axes = plt.subplots(nrows=3, ncols=3, figsize=(30, 10))
    axes = axes.ravel() 

    preselection = df.where(df.Strategy.isin(strategy_colors))
    by_file = preselection.groupby(["FileName"])

    for (filename, file_df), ax in zip(by_file, axes):
        user = file_df["UserType"].iloc[0]

        strategy = file_df['Strategy'].iloc[0]  # Strategy is the same for all rows of this user
        if strategy not in strategy_colors:
            continue
        file_df = file_df.sort_values('Time').reset_index()

        file_df["amount_signed"] = np.where(file_df.Operation.isin(["WITHDRAW_OP", "REFUND_OP"]), file_df.Amount, 0)
        file_df.amount_signed = np.where(file_df.Operation == "DEPOSIT_OP", -file_df.Amount, file_df.amount_signed)
        file_df["balance"] = file_df.amount_signed.cumsum()

        ax.plot(file_df['Time'], file_df['balance'], label=strategy, color=strategy_colors[strategy])

        # Enhancing the plot
        ax.set_title(f"Balance for {user}/{strategy}")
        ax.set_xlabel("Time")
        ax.set_ylabel("Balance")
        ax.tick_params(axis='x')

    plt.tight_layout()
    plt.savefig("results/balance", bbox_inches='tight')
    plt.show()

def coin_count(df: pd.DataFrame, strategy_colors: dict):

    fig, axes = plt.subplots(nrows=3, ncols=3, figsize=(30, 10))
    axes = axes.ravel() 

    preselection = df.where(df.Strategy.isin(strategy_colors))
    by_file = preselection.groupby(["FileName"])

    for (filename, file_df), ax in zip(by_file, axes):
        user = file_df["UserType"].iloc[0]
        
        strategy = file_df['Strategy'].iloc[0]  # Strategy is the same for all rows of this user
        if strategy not in strategy_colors:
            continue
        file_df = file_df.sort_values('Time').reset_index()

        ax.plot(file_df['Time'], file_df['CoinCount'], label=strategy, color=strategy_colors[strategy])

        # Enhancing the plot
        ax.set_title(f"Balance for {user}/{strategy}")
        ax.set_xlabel("Time")
        ax.set_ylabel("Balance")
        ax.tick_params(axis='x')

    plt.tight_layout()
    plt.savefig("results/coin_count", bbox_inches='tight')
    plt.show()


def cumulative_fee(df: pd.DataFrame):
    # Calculate cumulative fees for each file within each strategy
    df["CumulativeFee"] = df.groupby(["FileName", "UserType", "Strategy"])[
        "Fee"
    ].cumsum()

    # Create a pivot table for the final cumulative fee for each strategy and user type

    final_cumulative_fees = df.drop_duplicates(
        ["FileName", "UserType", "Strategy"], keep="last"
    )
    # Pivot table adjustment for easier access in the new plot setup
    pivot_df_new = final_cumulative_fees.pivot_table(
        index="UserType", columns="Strategy", values="CumulativeFee", aggfunc=list
    )

    # Plotting setup
    fig, axes = plt.subplots(nrows=2, ncols=4, figsize=(50, 15))
    axes = axes.ravel()

    for idx, (user_type, ax) in enumerate(zip(pivot_df_new.index, axes)):
        # Prepare boxplot data; collect lists from each cell in row corresponding to user type
        boxplot_data = [
            pivot_df_new.loc[user_type, strategy] for strategy in pivot_df_new.columns
        ]

        print(boxplot_data)

        ax.boxplot(
            boxplot_data, tick_labels=pivot_df_new.columns, showfliers=False
        )  # Optionally add `showfliers=False` to hide outliers
        ax.set_title(f"User Type: {user_type}", fontsize=26)
        ax.set_xlabel("Strategy", fontsize=22)
        ax.set_ylabel("Cumulative Fee", fontsize=22)
        ax.tick_params(axis="x", rotation=45, labelsize=13)

    plt.tight_layout()
    plt.savefig("results/final_cumulative_fees", bbox_inches="tight")
    plt.show()
