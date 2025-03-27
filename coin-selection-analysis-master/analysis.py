from src import common, graphs

colors, df, _ = common.init_dataframe()
graphs.coin_count(df, colors)
