from src import common, graphs

colors, df, _ = common.init_dataframe()
graphs.balance(df, colors)
