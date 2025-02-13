from pulp import LpVariable, lpSum, LpProblem, LpMinimize, PULP_CBC_CMD
from dataclasses import dataclass, field

@dataclass
class Fees:
    deposit_fee: int
    refund_fee: int
    withdraw_fee: int
    refresh_fee: int

@dataclass
class Rules:
    fees: Fees

@dataclass
class Denomination:
    name: str   
    amount: int
    rules: Rules

@dataclass
class Coin:
    unique_id: int
    denomination: Denomination

@dataclass
class Wallet:
    coins: list[Coin]
    # global fees

def parse_rules(rsa_keysize, cipher, fees, durations) -> Rules:
    return Rules(fees=Fees(*(satoshi for satoshi, percentage in fees)))

def parse_denomination(name, amount, rules_data) -> Denomination:
    return Denomination(name, amount, parse_rules(*rules_data))

def parse_coin(unique_id, denomination_data, creation_time) -> Coin:
    return Coin(unique_id, parse_denomination(*denomination_data))

def parse_wallet(wallet_data) -> Wallet:
    return Wallet(coins=[parse_coin(*coin_data) for coin_data in wallet_data["coins"]])


def select_minimize_fee(amount, wallet_data):
    wallet = parse_wallet(wallet_data)

    ncoins = len(wallet.coins)
    print("ncoins", ncoins)

    print("Amount", amount)
    print(sum([c.denomination.amount for c in wallet.coins]))
    print()

    selection = LpVariable.dicts("Selection", range(ncoins), cat="Binary")

    prob = LpProblem("CoinSelection", LpMinimize)

# Objective Funtion
    prob += (
            lpSum([coin.denomination.rules.fees.deposit_fee * selection[i]
                  for i, coin in enumerate(wallet.coins)]),
            "Denomination Fee")

# Constraints
    prob += (
            lpSum([wallet.coins[i].denomination.amount * selection[i] for i, coin in enumerate(wallet.coins)]) >= amount,
            "Amount is covered")
    solve_result = prob.solve(PULP_CBC_CMD(msg=False))

    print("Solve Result: ", solve_result)
    amounts = [c.denomination.amount for c in wallet.coins]
    print("Amounts: ", min(amounts), len(amounts), max(amounts))


# Each of the variables is printed with it's resolved optimum value
    for v in prob.variables():
        print(v.name, "=", v.varValue)
    selectedCoins = [k for k, v in selection.items() if v]
    

    print("Spending: ", sum([coin.denomination.amount for i, coin in enumerate(wallet.coins) if i in selectedCoins]))
    print("returning", [(i, wallet.coins[i].denomination.amount) for i, (k, v) in enumerate(selection.items()) if v])

    return [i for i, v in selection.items() if v]


