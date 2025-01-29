import csv, json, re
from pathlib import Path, PosixPath
from dataclasses import dataclass


results_path = Path("../coin-selection-c-master/simulation/results")
wallet_path = Path("../coin-selection-c-master/data/keys.json")
file_pattern = re.compile(r"user_(\d*)_strategy_(.*)\.csv")

@dataclass
class ResultFile:
    user_id: int
    strategy: str
    file: PosixPath

    @staticmethod
    def from_path(path):
        match = file_pattern.match(path.name)
        
        if not match:
            return None
    
        u_id, strat = match.groups()
        return ResultFile(user_id=int(u_id), strategy=strat, file=path)

@dataclass
class LineItem:
    item_id: int
    time: int
    amount: int
    op: str
    fees: int

@dataclass
class Coin:
    amount: int
    currency: str

    fee_withdraw: int
    fee_deposit: int
    fee_refresh: int
    fee_refund: int

    expire_withdraw: int
    expire_deposit: int
    expire_legal: int

def all_files():
    return [f for f in results_path.iterdir() if f.name.endswith(".csv")]

def read_file(path):
    with open(path) as csv_file:
        reader = csv.reader(csv_file, delimiter=',', skipinitialspace=True)

        for n, line in enumerate(reader):
            if n:
                item, time, amount, op, fees = line

                if op not in {"DEPOSIT_OP", "REFRESH_OP", "DEPOSIT_REFRESH_OP"}:
                    continue

                item_id = int(item.split("_")[-1])

                yield LineItem(item_id, time=int(time), amount=int(amount), op=op, fees=int(fees))

def parse_wallet():
    with open(wallet_path) as wallet_file:
        data = json.load(wallet_file)
       
        currency = data["currency"]
        print(f"Wallet currency: {currency}")

        denominations = data["denominations"]

        print(f"{len(denominations)} denominations")

        coins = []
        for denomination in denominations:
            amount = denomination["value"]
            fee_withdraw = denomination["fee_withdraw"]
            fee_deposit = denomination["fee_deposit"]
            fee_refresh = denomination["fee_refresh"]
            fee_refund = denomination["fee_refund"]
            cipher = denomination["cipher"]

            denom = denomination["denoms"][0]
            stamp_start = denom["stamp_start"]["t_s"]

            expire_withdraw = denom["stamp_expire_withdraw"]["t_s"] - stamp_start
            expire_deposit = denom["stamp_expire_deposit"]["t_s"] - stamp_start
            expire_legal = denom["stamp_expire_legal"]["t_s"] - stamp_start


            #coin = Coin(amount=amount, currency=currency, fee_withdraw=fee_withdraw, fee_deposit=fee_deposit) 
            print(expire_withdraw)
            
            break



def process_files():
    
    for file in all_files()[:1]:
        content = read_file(file)
        for c in content:
            print(c)
            break
    
    parse_wallet()

process_files()
